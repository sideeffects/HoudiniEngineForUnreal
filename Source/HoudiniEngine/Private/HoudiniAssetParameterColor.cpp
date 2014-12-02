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


UHoudiniAssetParameterColor::UHoudiniAssetParameterColor(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{
	Color = FColor::White;
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


void
UHoudiniAssetParameterColor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


UHoudiniAssetParameterColor*
UHoudiniAssetParameterColor::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	UHoudiniAssetParameterColor* HoudiniAssetParameterColor = NewObject<UHoudiniAssetParameterColor>(InHoudiniAssetComponent);

	HoudiniAssetParameterColor->CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo);
	return HoudiniAssetParameterColor;
}


bool
UHoudiniAssetParameterColor::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InNodeId, ParmInfo))
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
	if(HAPI_RESULT_SUCCESS != HAPI_GetParmFloatValues(InNodeId, (float*) &Color.R, ValuesIndex, TupleSize))
	{
		return false;
	}

	if(TupleSize == 3)
	{
		Color.A = 1.0f;
	}

	return true;
}


void
UHoudiniAssetParameterColor::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(GetParameterLabel())
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	
	TSharedPtr<SColorBlock> ColorBlock = SNew(SColorBlock);
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ColorBlock, SColorBlock)
		.Color(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateUObject(this, &UHoudiniAssetParameterColor::GetColor)))
	];

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


bool
UHoudiniAssetParameterColor::UploadParameterValue()
{
	if(HAPI_RESULT_SUCCESS != HAPI_SetParmFloatValues(NodeId, (const float*) &Color.R, ValuesIndex, TupleSize))
	{
		return false;
	}

	return Super::UploadParameterValue();
}


FLinearColor
UHoudiniAssetParameterColor::GetColor() const
{
	return Color;
}

