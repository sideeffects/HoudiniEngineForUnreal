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
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterColor::UHoudiniAssetParameterColor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	Color(FColor::White),
	bIsColorPickerOpen(false)
{

}


UHoudiniAssetParameterColor::~UHoudiniAssetParameterColor()
{

}


void
UHoudiniAssetParameterColor::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		Color = FColor::White;
	}

	Ar << Color;
}


UHoudiniAssetParameterColor*
UHoudiniAssetParameterColor::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterColor* HoudiniAssetParameterColor = NewObject<UHoudiniAssetParameterColor>(Outer,
		UHoudiniAssetParameterColor::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterColor->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterColor;
}


bool
UHoudiniAssetParameterColor::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle float type.
	if(HAPI_PARMTYPE_COLOR != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.floatValuesIndex);

	// Get the actual value for this property.
	Color = FLinearColor::White;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(), InNodeId,
		(float*)&Color.R, ValuesIndex, TupleSize))
	{
		return false;
	}

	if(TupleSize == 3)
	{
		Color.A = 1.0f;
	}

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterColor::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	ColorBlock.Reset();

	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);

	ColorBlock = SNew(SColorBlock);
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ColorBlock, SColorBlock)
		.Color(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateUObject(this,
			&UHoudiniAssetParameterColor::GetColor)))
		.OnMouseButtonDown(FPointerEventHandler::CreateUObject(this,
			&UHoudiniAssetParameterColor::OnColorBlockMouseButtonDown))
	];

	if(ColorBlock.IsValid())
	{
		ColorBlock->SetEnabled(!bIsDisabled);
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

#endif


bool
UHoudiniAssetParameterColor::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmFloatValues(
		FHoudiniEngine::Get().GetSession(), NodeId, (const float*)&Color.R, ValuesIndex, TupleSize))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


bool
UHoudiniAssetParameterColor::SetParameterVariantValue(const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo)
{
	int32 VariantType = Variant.GetType();
	FLinearColor VariantLinearColor = FLinearColor::White;

	if(EVariantTypes::Color == VariantType)
	{
		FColor VariantColor = Variant.GetValue<FColor>();
		VariantLinearColor = VariantColor.ReinterpretAsLinear();
	}
	else if(EVariantTypes::LinearColor == VariantType)
	{
		VariantLinearColor = Variant.GetValue<FLinearColor>();
	}
	else
	{
		return false;
	}

#if WITH_EDITOR

	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniAssetParameterColorChange", "Houdini Parameter Color: Changing a value"),
		HoudiniAssetComponent);

	Modify();

	if(!bRecordUndo)
	{
		Transaction.Cancel();
	}

#endif

	MarkPreChanged(bTriggerModify);
	Color = VariantLinearColor;
	MarkChanged(bTriggerModify);

	return true;
}


FLinearColor
UHoudiniAssetParameterColor::GetColor() const
{
	return Color;
}


#if WITH_EDITOR

bool
UHoudiniAssetParameterColor::IsColorPickerWindowOpen() const
{
	return bIsColorPickerOpen;
}


FReply
UHoudiniAssetParameterColor::OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	FColorPickerArgs PickerArgs;
	PickerArgs.ParentWidget = ColorBlock;
	PickerArgs.bUseAlpha = true;
	PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine,
		&UEngine::GetDisplayGamma));
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateUObject(this,
		&UHoudiniAssetParameterColor::OnPaintColorChanged, true, true);
	PickerArgs.InitialColorOverride = GetColor();
	PickerArgs.bOnlyRefreshOnOk = true;
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateUObject(this,
		&UHoudiniAssetParameterColor::OnColorPickerClosed);

	OpenColorPicker(PickerArgs);
	bIsColorPickerOpen = true;

	return FReply::Handled();
}

#endif


void
UHoudiniAssetParameterColor::OnPaintColorChanged(FLinearColor InNewColor, bool bTriggerModify, bool bRecordUndo)
{
	if(Color != InNewColor)
	{

#if WITH_EDITOR

		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterColorChange", "Houdini Parameter Color: Changing a value"),
			HoudiniAssetComponent);
		Modify();

		if(!bRecordUndo)
		{
			Transaction.Cancel();
		}

#endif

		MarkPreChanged(bTriggerModify);

		Color = InNewColor;

		// Mark this parameter as changed.
		MarkChanged(bTriggerModify);
	}
}


void
UHoudiniAssetParameterColor::OnColorPickerClosed(const TSharedRef<SWindow>& Window)
{
	bIsColorPickerOpen = false;
}
