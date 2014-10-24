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


UHoudiniAssetParameterString::UHoudiniAssetParameterString(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{

}


UHoudiniAssetParameterString::~UHoudiniAssetParameterString()
{

}


UHoudiniAssetParameterString*
UHoudiniAssetParameterString::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UHoudiniAssetParameterString* HoudiniAssetParameterString = new UHoudiniAssetParameterString(FPostConstructInitializeProperties());
	HoudiniAssetParameterString->CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo);
	return HoudiniAssetParameterString;
}


bool
UHoudiniAssetParameterString::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle integer type.
	if(HAPI_PARMTYPE_STRING != ParmInfo.type)
	{
		return false;
	}

	// Get the actual value for this property.
	HAPI_StringHandle StringHandle;
	if(HAPI_RESULT_SUCCESS != HAPI_GetParmStringValues(InNodeId, false, &StringHandle, ParmInfo.stringValuesIndex, 1))
	{
		return false;
	}

	// Convert HAPI string handle to Unreal text.
	FString ValueString;
	if(!FHoudiniEngineUtils::GetHoudiniString(StringHandle, ValueString))
	{
		return false;
	}

	Value = FText::FromString(ValueString);

	return true;
}


void
UHoudiniAssetParameterString::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
}


void
UHoudiniAssetParameterString::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetParameterString::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(Label)
							.ToolTipText(Label)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	Row.ValueWidget.Widget = SNew(SEditableTextBox)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							
							.Text(Value)
							.OnTextChanged(FOnTextChanged::CreateUObject(this, &UHoudiniAssetParameterString::SetValue))
							.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UHoudiniAssetParameterString::SetValueCommitted));
}


bool
UHoudiniAssetParameterString::UploadParameterValue()
{
	HAPI_ParmInfo ParmInfo;
	if(HAPI_RESULT_SUCCESS != HAPI_GetParameters(NodeId, &ParmInfo, ParmId, 1))
	{
		return false;
	}

	std::string String = TCHAR_TO_UTF8(*(Value.ToString()));
	if(HAPI_RESULT_SUCCESS != HAPI_SetParmStringValue(NodeId, String.c_str(), ParmId, 0))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


void
UHoudiniAssetParameterString::SetValue(const FText& InValue)
{

}


void
UHoudiniAssetParameterString::SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType)
{
	Value = InValue;

	// Mark this parameter as changed.
	MarkChanged();
}
