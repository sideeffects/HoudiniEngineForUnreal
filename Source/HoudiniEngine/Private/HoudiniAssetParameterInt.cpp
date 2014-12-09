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


UHoudiniAssetParameterInt::UHoudiniAssetParameterInt(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ValueMin(TNumericLimits<int32>::Lowest()),
	ValueMax(TNumericLimits<int32>::Max()),
	ValueUIMin(TNumericLimits<int32>::Lowest()),
	ValueUIMax(TNumericLimits<int32>::Max())
{
	// Parameter will have at least one value.
	Values.AddZeroed(1);
}


UHoudiniAssetParameterInt::~UHoudiniAssetParameterInt()
{

}


UHoudiniAssetParameterInt*
UHoudiniAssetParameterInt::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
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

	UHoudiniAssetParameterInt* HoudiniAssetParameterInt = NewObject<UHoudiniAssetParameterInt>(Outer);

	HoudiniAssetParameterInt->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterInt;
}


bool
UHoudiniAssetParameterInt::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, 
										   HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle integer type.
	if(HAPI_PARMTYPE_INT != ParmInfo.type)
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

	// Set min and max for this property.
	if(ParmInfo.hasMin)
	{
		ValueMin = (int32) ParmInfo.min;
	}

	if(ParmInfo.hasMax)
	{
		ValueMax = (int32) ParmInfo.max;
	}

	// Set min and max for UI for this property.
	if(ParmInfo.hasUIMin)
	{
		ValueUIMin = (int32) ParmInfo.UIMin;
	}
	else
	{
		// If it is not set, use supplied min.
		ValueUIMin = ValueMin;
	}

	if(ParmInfo.hasUIMax)
	{
		ValueUIMax = (int32) ParmInfo.UIMax;
	}
	else
	{
		// If it is not set, use supplied max.
		ValueUIMax = ValueMax;
	}

	return true;
}


void
UHoudiniAssetParameterInt::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(GetParameterLabel())
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SNumericEntryBox<int>)
			.AllowSpin(true)

			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

			.MinValue(ValueMin)
			.MaxValue(ValueMax)

			.MinSliderValue(ValueUIMin)
			.MaxSliderValue(ValueUIMax)

			.Value(TAttribute<TOptional<int32> >::Create(TAttribute<TOptional<int32> >::FGetter::CreateUObject(this, &UHoudiniAssetParameterInt::GetValue, Idx)))
			.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterInt::SetValue, Idx))
			.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateUObject(this, &UHoudiniAssetParameterInt::SetValueCommitted, Idx))
			.OnBeginSliderMovement(FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterInt::OnSliderMovingBegin, Idx))
			.OnEndSliderMovement(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterInt::OnSliderMovingFinish, Idx))

			.SliderExponent(1.0f)
		];
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


bool
UHoudiniAssetParameterInt::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != HAPI_SetParmIntValues(NodeId, &Values[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


TOptional<int32>
UHoudiniAssetParameterInt::GetValue(int32 Idx) const
{
	return TOptional<int32>(Values[Idx]);
}


void
UHoudiniAssetParameterInt::SetValue(int32 InValue, int32 Idx)
{
	if(Values[Idx] != InValue)
	{
		MarkPreChanged();

		Values[Idx] = FMath::Clamp<int32>(InValue, ValueMin, ValueMax);

		// Mark this parameter as changed.
		MarkChanged();
	}
}


void
UHoudiniAssetParameterInt::SetValueCommitted(int32 InValue, ETextCommit::Type CommitType, int32 Idx)
{

}


void
UHoudiniAssetParameterInt::OnSliderMovingBegin(int32 Idx)
{

}


void
UHoudiniAssetParameterInt::OnSliderMovingFinish(int32 InValue, int32 Idx)
{

}


void
UHoudiniAssetParameterInt::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		Values.Empty();
	}

	Ar << Values;

	Ar << ValueMin;
	Ar << ValueMax;

	Ar << ValueUIMin;
	Ar << ValueUIMax;
}


void
UHoudiniAssetParameterInt::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}

