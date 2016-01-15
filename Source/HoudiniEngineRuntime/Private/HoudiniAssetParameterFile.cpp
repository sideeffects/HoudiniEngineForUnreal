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
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterFile::UHoudiniAssetParameterFile(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	Values.Add(TEXT(""));
}


UHoudiniAssetParameterFile::~UHoudiniAssetParameterFile()
{

}


UHoudiniAssetParameterFile*
UHoudiniAssetParameterFile::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterFile* HoudiniAssetParameterFile = NewObject<UHoudiniAssetParameterFile>(Outer,
		UHoudiniAssetParameterFile::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetParameterFile->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterFile;
}


bool
UHoudiniAssetParameterFile::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle file types.
	switch(ParmInfo.type)
	{
		case HAPI_PARMTYPE_PATH_FILE:
		case HAPI_PARMTYPE_PATH_FILE_GEO:
		case HAPI_PARMTYPE_PATH_FILE_IMAGE:
		{
			break;
		}

		default:
		{
			return false;
		}
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
		FHoudiniEngineUtils::GetHoudiniString(StringHandles[Idx], ValueString);
		Values[Idx] = ValueString;
	}

	return true;
}


void
UHoudiniAssetParameterFile::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << Values;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterFile::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
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
			.OnTextChanged(FOnTextChanged::CreateUObject(this, &UHoudiniAssetParameterFile::SetValue, Idx))
			.OnTextCommitted(FOnTextCommitted::CreateUObject(this,
				&UHoudiniAssetParameterFile::SetValueCommitted, Idx))
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
UHoudiniAssetParameterFile::UploadParameterValue()
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


#if WITH_EDITOR

void
UHoudiniAssetParameterFile::SetValue(const FText& InValue, int32 Idx)
{

}


void
UHoudiniAssetParameterFile::SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 Idx)
{
	FString CommittedValue = InValue.ToString();

	if(Values[Idx] != CommittedValue)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniAssetParameterFileChange", "Houdini Parameter File: Changing a value"),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		Values[Idx] = CommittedValue;

		// Mark this parameter as changed.
		MarkChanged();
	}
}

#endif
