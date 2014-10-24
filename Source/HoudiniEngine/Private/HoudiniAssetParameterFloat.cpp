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
	Value(0.0f),
	ValueMin(TNumericLimits<float>::Lowest()),
	ValueMax(TNumericLimits<float>::Max()),
	ValueUIMin(TNumericLimits<float>::Lowest()),
	ValueUIMax(TNumericLimits<float>::Max())
{

}


UHoudiniAssetParameterFloat::~UHoudiniAssetParameterFloat()
{

}


UHoudiniAssetParameterFloat*
UHoudiniAssetParameterFloat::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UHoudiniAssetParameterFloat* HoudiniAssetParameterFloat = new UHoudiniAssetParameterFloat(FPostConstructInitializeProperties());
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

	// We can only handle integer type.
	if(HAPI_PARMTYPE_FLOAT != ParmInfo.type)
	{
		return false;
	}

	// Get the actual value for this property.
	if(HAPI_RESULT_SUCCESS != HAPI_GetParmFloatValues(InNodeId, &Value, ParmInfo.floatValuesIndex, 1))
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
							.Text(Label)
							.ToolTipText(Label)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	Row.ValueWidget.Widget = SNew(SNumericEntryBox<float>)
							.AllowSpin(true)

							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

							.MinValue(ValueMin)
							.MaxValue(ValueMax)

							.MinSliderValue(ValueUIMin)
							.MaxSliderValue(ValueUIMax)

							.Value(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this, &UHoudiniAssetParameterFloat::GetValue)))
							.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterFloat::SetValue))
							.OnValueCommitted(SNumericEntryBox<float>::FOnValueCommitted::CreateUObject(this, &UHoudiniAssetParameterFloat::SetValueCommitted))
							.OnBeginSliderMovement(FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterFloat::OnSliderMovingBegin))
							.OnEndSliderMovement(SNumericEntryBox<float>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterFloat::OnSliderMovingFinish))

							.SliderExponent(1.0f);
}


bool
UHoudiniAssetParameterFloat::UploadParameterValue()
{
	HAPI_ParmInfo ParmInfo;
	if(HAPI_RESULT_SUCCESS != HAPI_GetParameters(NodeId, &ParmInfo, ParmId, 1))
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != HAPI_SetParmFloatValues(NodeId, &Value, ParmInfo.floatValuesIndex, ParmInfo.size))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


TOptional<float>
UHoudiniAssetParameterFloat::GetValue() const
{
	return TOptional<float>(Value);
}


void
UHoudiniAssetParameterFloat::SetValue(float InValue)
{
	Value = InValue;

	// Mark this parameter as changed.
	MarkChanged();
}


void
UHoudiniAssetParameterFloat::SetValueCommitted(float InValue, ETextCommit::Type CommitType)
{

}


void
UHoudiniAssetParameterFloat::OnSliderMovingBegin()
{

}


void
UHoudiniAssetParameterFloat::OnSliderMovingFinish(float InValue)
{

}


void
UHoudiniAssetParameterFloat::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
}


void
UHoudiniAssetParameterFloat::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}
