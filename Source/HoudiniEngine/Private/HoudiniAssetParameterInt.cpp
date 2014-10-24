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


UHoudiniAssetParameterInt::UHoudiniAssetParameterInt(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	Value(0),
	ValueMin(TNumericLimits<int32>::Lowest()),
	ValueMax(TNumericLimits<int32>::Max()),
	ValueUIMin(TNumericLimits<int32>::Lowest()),
	ValueUIMax(TNumericLimits<int32>::Max())
{

}


UHoudiniAssetParameterInt::~UHoudiniAssetParameterInt()
{

}


UHoudiniAssetParameterInt*
UHoudiniAssetParameterInt::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UHoudiniAssetParameterInt* HoudiniAssetParameterInt = new UHoudiniAssetParameterInt(FPostConstructInitializeProperties());
	HoudiniAssetParameterInt->CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo);
	return HoudiniAssetParameterInt;
}


bool
UHoudiniAssetParameterInt::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle integer type.
	if(HAPI_PARMTYPE_INT != ParmInfo.type)
	{
		return false;
	}

	// Get the actual value for this property.
	if(HAPI_RESULT_SUCCESS != HAPI_GetParmIntValues(InNodeId, &Value, ParmInfo.intValuesIndex, 1))
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
							.Text(Label)
							.ToolTipText(Label)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	Row.ValueWidget.Widget = SNew(SNumericEntryBox<int>)
							.AllowSpin(true)

							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

							.MinValue(ValueMin)
							.MaxValue(ValueMax)

							.MinSliderValue(ValueUIMin)
							.MaxSliderValue(ValueUIMax)

							.Value(TAttribute<TOptional<int32> >::Create(TAttribute<TOptional<int32> >::FGetter::CreateUObject(this, &UHoudiniAssetParameterInt::GetValue)))
							.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterInt::SetValue))
							.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateUObject(this, &UHoudiniAssetParameterInt::SetValueCommitted))
							.OnBeginSliderMovement(FSimpleDelegate::CreateUObject(this, &UHoudiniAssetParameterInt::OnSliderMovingBegin))
							.OnEndSliderMovement(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(this, &UHoudiniAssetParameterInt::OnSliderMovingFinish))

							.SliderExponent(1.0f);
}


bool
UHoudiniAssetParameterInt::UploadParameterValue()
{
	HAPI_ParmInfo ParmInfo;
	if(HAPI_RESULT_SUCCESS != HAPI_GetParameters(NodeId, &ParmInfo, ParmId, 1))
	{
		return false;
	}

	if(HAPI_RESULT_SUCCESS != HAPI_SetParmIntValues(NodeId, &Value, ParmInfo.intValuesIndex, ParmInfo.size))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


TOptional<int32>
UHoudiniAssetParameterInt::GetValue() const
{
	return TOptional<int32>(Value);
}


void
UHoudiniAssetParameterInt::SetValue(int32 InValue)
{
	Value = InValue;

	// Mark this parameter as changed.
	MarkChanged();
}


void
UHoudiniAssetParameterInt::SetValueCommitted(int32 InValue, ETextCommit::Type CommitType)
{

}


void
UHoudiniAssetParameterInt::OnSliderMovingBegin()
{

}


void
UHoudiniAssetParameterInt::OnSliderMovingFinish(int32 InValue)
{

}


void
UHoudiniAssetParameterInt::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
}


void
UHoudiniAssetParameterInt::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}
