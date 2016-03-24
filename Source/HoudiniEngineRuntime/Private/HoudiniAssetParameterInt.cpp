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
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"


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
UHoudiniAssetParameterInt::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterInt* HoudiniAssetParameterInt = NewObject<UHoudiniAssetParameterInt>(Outer,
		UHoudiniAssetParameterInt::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterInt->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterInt;
}


bool
UHoudiniAssetParameterInt::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
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
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmIntValues(FHoudiniEngine::Get().GetSession(), InNodeId, &Values[0],
		ValuesIndex, TupleSize))
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
		if(ParmInfo.hasMin)
		{
			ValueUIMin = ValueMin;
		}
		else
		{
			// Min value Houdini uses by default.
			ValueUIMin = HAPI_UNREAL_PARAM_INT_UI_MIN;
		}
	}

	if(ParmInfo.hasUIMax)
	{
		ValueUIMax = (int32) ParmInfo.UIMax;
	}
	else
	{
		// If it is not set, use supplied max.
		if(ParmInfo.hasMax)
		{
			ValueUIMax = ValueMax;
		}
		else
		{
			// Max value Houdini uses by default.
			ValueUIMax = HAPI_UNREAL_PARAM_INT_UI_MAX;
		}
	}

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterInt::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		TSharedPtr<SNumericEntryBox<int32> > NumericEntryBox;

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(NumericEntryBox, SNumericEntryBox<int32>)
			.AllowSpin(true)

			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

			.MinValue(ValueMin)
			.MaxValue(ValueMax)

			.MinSliderValue(ValueUIMin)
			.MaxSliderValue(ValueUIMax)

			.Value(TAttribute<TOptional<int32> >::Create(
				TAttribute<TOptional<int32> >::FGetter::CreateUObject(this,
					&UHoudiniAssetParameterInt::GetValue, Idx)))
			.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(
				this, &UHoudiniAssetParameterInt::SetValue, Idx, true, true))
			.OnValueCommitted(SNumericEntryBox<int32>::FOnValueCommitted::CreateUObject(
				this, &UHoudiniAssetParameterInt::SetValueCommitted, Idx))
			.OnBeginSliderMovement(FSimpleDelegate::CreateUObject(
				this, &UHoudiniAssetParameterInt::OnSliderMovingBegin, Idx))
			.OnEndSliderMovement(SNumericEntryBox<int32>::FOnValueChanged::CreateUObject(
				this, &UHoudiniAssetParameterInt::OnSliderMovingFinish, Idx))

			.SliderExponent(1.0f)
		];

		if(NumericEntryBox.IsValid())
		{
			NumericEntryBox->SetEnabled(!bIsDisabled);
		}
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

#endif


bool
UHoudiniAssetParameterInt::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId, &Values[0],
		ValuesIndex, TupleSize))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


bool
UHoudiniAssetParameterInt::SetParameterVariantValue(const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo)
{
	int32 VariantType = Variant.GetType();
	int32 VariantValue = 0;

	if(Idx >= 0 && Idx < Values.Num())
	{
		return false;
	}

	switch(VariantType)
	{
		case EVariantTypes::Int8:
		case EVariantTypes::Int16:
		case EVariantTypes::Int32:
		case EVariantTypes::Int64:
		case EVariantTypes::UInt8:
		case EVariantTypes::UInt16:
		case EVariantTypes::UInt32:
		case EVariantTypes::UInt64:
		{
			VariantValue = Variant.GetValue<int32>();
			break;
		}

		default:
		{
			return false;
		}
	}

#if WITH_EDITOR

	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterIntChange", "Houdini Parameter Integer: Changing a value"),
			HoudiniAssetComponent);

	Modify();

	if(!bRecordUndo)
	{
		Transaction.Cancel();
	}

#endif

	MarkPreChanged(bTriggerModify);
	Values[Idx] = VariantValue;
	MarkChanged(bTriggerModify);

	return true;
}


TOptional<int32>
UHoudiniAssetParameterInt::GetValue(int32 Idx) const
{
	return TOptional<int32>(Values[Idx]);
}


void
UHoudiniAssetParameterInt::SetValue(int32 InValue, int32 Idx, bool bTriggerModify, bool bRecordUndo)
{
	if(Values[Idx] != InValue)
	{

#if WITH_EDITOR

	// If this is not a slider change (user typed in values manually), record undo information.
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterIntChange", "Houdini Parameter Integer: Changing a value"),
		HoudiniAssetComponent);
	Modify();

	if(bSliderDragged || !bRecordUndo)
	{
		Transaction.Cancel();
	}

#endif

		MarkPreChanged(bTriggerModify);

		Values[Idx] = FMath::Clamp<int32>(InValue, ValueMin, ValueMax);

		// Mark this parameter as changed.
		MarkChanged(bTriggerModify);
	}
}


int32
UHoudiniAssetParameterInt::GetParameterValue(int32 Idx, int32 DefaultValue) const
{
	if(Values.Num() <= Idx)
	{
		return DefaultValue;
	}

	return Values[Idx];
}


#if WITH_EDITOR

void
UHoudiniAssetParameterInt::SetValueCommitted(int32 InValue, ETextCommit::Type CommitType, int32 Idx)
{

}


void
UHoudiniAssetParameterInt::OnSliderMovingBegin(int32 Idx)
{
	// We want to record undo increments only when user lets go of the slider.
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterIntChange", "Houdini Parameter Integer: Changing a value"),
		HoudiniAssetComponent);
	Modify();

	bSliderDragged = true;
}


void
UHoudiniAssetParameterInt::OnSliderMovingFinish(int32 InValue, int32 Idx)
{
	bSliderDragged = false;
}

#endif


void
UHoudiniAssetParameterInt::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << Values;

	Ar << ValueMin;
	Ar << ValueMax;

	Ar << ValueUIMin;
	Ar << ValueUIMax;
}
