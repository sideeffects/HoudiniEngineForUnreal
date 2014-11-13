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


UHoudiniAssetParameterFloat::UHoudiniAssetParameterFloat(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	ValueMin(TNumericLimits<float>::Lowest()),
	ValueMax(TNumericLimits<float>::Max()),
	ValueUIMin(TNumericLimits<float>::Lowest()),
	ValueUIMax(TNumericLimits<float>::Max())
{
	// Parameter will have at least one value.
	Values.AddZeroed(1);
}


UHoudiniAssetParameterFloat::~UHoudiniAssetParameterFloat()
{

}


void
UHoudiniAssetParameterFloat::Serialize(FArchive& Ar)
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
UHoudiniAssetParameterFloat::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


UHoudiniAssetParameterFloat*
UHoudiniAssetParameterFloat::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UHoudiniAssetParameterFloat* HoudiniAssetParameterFloat = NewObject<UHoudiniAssetParameterFloat>(InHoudiniAssetComponent);

	HoudiniAssetParameterFloat->CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo);
	return HoudiniAssetParameterFloat;
}


bool
UHoudiniAssetParameterFloat::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle float type.
	if(HAPI_PARMTYPE_FLOAT != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.floatValuesIndex);

	// Get the actual value for this property.
	Values.SetNumZeroed(TupleSize);
	if(HAPI_RESULT_SUCCESS != HAPI_GetParmFloatValues(InNodeId, &Values[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	// Set min and max for this property.
	if(ParmInfo.hasMin)
	{
		ValueMin = ParmInfo.min;
	}

	if(ParmInfo.hasMax)
	{
		ValueMax = ParmInfo.max;
	}

	// Set min and max for UI for this property.
	if(ParmInfo.hasUIMin)
	{
		ValueUIMin = ParmInfo.UIMin;
	}
	else
	{
		// If it is not set, use supplied min.
		ValueUIMin = ValueMin;
	}

	if(ParmInfo.hasUIMax)
	{
		ValueUIMax = ParmInfo.UIMax;
	}
	else
	{
		// If it is not set, use supplied max.
		ValueUIMax = ValueMax;
	}

	return true;
}


void
UHoudiniAssetParameterFloat::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
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
			SNew(SNumericEntryBox<float>)
			.AllowSpin(true)

			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

			.MinValue(ValueMin)
			.MaxValue(ValueMax)

			.MinSliderValue(ValueUIMin)
			.MaxSliderValue(ValueUIMax)

			.Value(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this, &UHoudiniAssetParameterFloat::GetValue, Idx)))
			.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterFloat::SetValue, Idx))
			.OnValueCommitted(SNumericEntryBox<float>::FOnValueCommitted::CreateUObject(this, &UHoudiniAssetParameterFloat::SetValueCommitted, Idx))
			.OnBeginSliderMovement(FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterFloat::OnSliderMovingBegin, Idx))
			.OnEndSliderMovement(SNumericEntryBox<float>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterFloat::OnSliderMovingFinish, Idx))

			.SliderExponent(1.0f)
		];
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


bool
UHoudiniAssetParameterFloat::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != HAPI_SetParmFloatValues(NodeId, &Values[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


TOptional<float>
UHoudiniAssetParameterFloat::GetValue(int32 Idx) const
{
	return TOptional<float>(Values[Idx]);
}


void
UHoudiniAssetParameterFloat::SetValue(float InValue, int32 Idx)
{
	if(Values[Idx] != InValue)
	{
		MarkPreChanged();

		Values[Idx] = FMath::Clamp<float>(InValue, ValueMin, ValueMax);

		// Mark this parameter as changed.
		MarkChanged();
	}
}


void
UHoudiniAssetParameterFloat::SetValueCommitted(float InValue, ETextCommit::Type CommitType, int32 Idx)
{

}


void
UHoudiniAssetParameterFloat::OnSliderMovingBegin(int32 Idx)
{

}


void
UHoudiniAssetParameterFloat::OnSliderMovingFinish(float InValue, int32 Idx)
{

}


void
UHoudiniAssetParameterFloat::PrintParameterInfo()
{
	switch(TupleSize)
	{
		case 3:
		{
			HOUDINI_LOG_MESSAGE(TEXT("Float Parameter Change: %s -> %f, %f, %f"), *ParameterName, Values[0], Values[1], Values[2]);
			break;
		}

		case 2:
		{
			HOUDINI_LOG_MESSAGE(TEXT("Float Parameter Change: %s -> %f, %f"), *ParameterName, Values[0], Values[1]);
			break;
		}

		case 1:
		{
			HOUDINI_LOG_MESSAGE(TEXT("Float Parameter Change: %s -> %f"), *ParameterName, Values[0]);
			break;
		}

		default:
		{
			break;
		}
	}
}
