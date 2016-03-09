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
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetParameterString.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


UHoudiniAssetParameterString::UHoudiniAssetParameterString(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	Values.Add(TEXT(""));
}


UHoudiniAssetParameterString::~UHoudiniAssetParameterString()
{

}


UHoudiniAssetParameterString*
UHoudiniAssetParameterString::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterString* HoudiniAssetParameterString = NewObject<UHoudiniAssetParameterString>(Outer,
		UHoudiniAssetParameterString::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterString->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterString;
}


bool
UHoudiniAssetParameterString::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle string type.
	if(HAPI_PARMTYPE_STRING != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.stringValuesIndex);

	// Get the actual value for this property.
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(TupleSize);
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmStringValues(
		FHoudiniEngine::Get().GetSession(), InNodeId, false, &StringHandles[0], ValuesIndex, TupleSize))
	{
		return false;
	}

	// Convert HAPI string handles to Unreal strings.
	Values.SetNum(TupleSize);
	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		FString ValueString = TEXT("");
		FHoudiniEngineString HoudiniEngineString(StringHandles[Idx]);
		HoudiniEngineString.ToFString(ValueString);
		Values[Idx] = ValueString;
	}

	return true;
}


void
UHoudiniAssetParameterString::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << Values;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterString::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	// Create the standard parameter name widget.
	CreateNameWidget(Row, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		TSharedPtr<SEditableTextBox> EditableTextBox;

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(EditableTextBox, SEditableTextBox)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

			.Text(FText::FromString(Values[Idx]))
			.OnTextChanged(FOnTextChanged::CreateUObject(this, &UHoudiniAssetParameterString::SetValue, Idx))
			.OnTextCommitted(FOnTextCommitted::CreateUObject(this,
				&UHoudiniAssetParameterString::SetValueCommitted, Idx))
		];

		if(EditableTextBox.IsValid())
		{
			EditableTextBox->SetEnabled(!bIsDisabled);
		}
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

#endif


bool
UHoudiniAssetParameterString::UploadParameterValue()
{
	for(int32 Idx = 0; Idx < Values.Num(); ++Idx)
	{
		std::string ConvertedString = TCHAR_TO_UTF8(*(Values[Idx]));
		if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmStringValue(
			FHoudiniEngine::Get().GetSession(), NodeId, ConvertedString.c_str(), ParmId, Idx))
		{
			return false;
		}
	}

	return Super::UploadParameterValue();
}


bool
UHoudiniAssetParameterString::SetParameterVariantValue(const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo)
{
	int32 VariantType = Variant.GetType();

	if(Idx >= 0 && Idx < Values.Num())
	{
		return false;
	}

	if(EVariantTypes::String == VariantType)
	{
		const FString& VariantStringValue = Variant.GetValue<FString>();

#if WITH_EDITOR

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterStringChange", "Houdini Parameter String: Changing a value"),
				HoudiniAssetComponent);

		Modify();

		if(!bRecordUndo)
		{
			Transaction.Cancel();
		}

#endif

		MarkPreChanged();
		Values[Idx] = VariantStringValue;
		MarkChanged();

		return true;
	}

	return false;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterString::SetValue(const FText& InValue, int32 Idx)
{

}


void
UHoudiniAssetParameterString::SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 Idx)
{
	FString CommittedValue = InValue.ToString();

	if(Values[Idx] != CommittedValue)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterStringChange", "Houdini Parameter String: Changing a value"),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		Values[Idx] = CommittedValue;

		// Mark this parameter as changed.
		MarkChanged();
	}
}

#endif
