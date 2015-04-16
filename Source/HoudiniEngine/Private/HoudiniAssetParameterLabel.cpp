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
#include "HoudiniAssetParameterLabel.h"

#if WITH_EDITOR
#include "HoudiniAssetComponentDetails.h"
#endif


UHoudiniAssetParameterLabel::UHoudiniAssetParameterLabel(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterLabel::~UHoudiniAssetParameterLabel()
{

}


UHoudiniAssetParameterLabel*
UHoudiniAssetParameterLabel::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterLabel* HoudiniAssetParameterLabel = NewObject<UHoudiniAssetParameterLabel>(Outer);

	HoudiniAssetParameterLabel->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterLabel;
}


bool
UHoudiniAssetParameterLabel::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle label type.
	if(HAPI_PARMTYPE_LABEL != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.stringValuesIndex);

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterLabel::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	TSharedPtr<STextBlock> TextBlock;

	DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
	[
		SAssignNew(TextBlock, STextBlock)
		.Text(GetParameterLabel())
		.ToolTipText(GetParameterLabel())
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.WrapTextAt(FHoudiniAssetComponentDetails::RowFullWidgetDesiredWidth)
	];

	if(TextBlock.IsValid())
	{
		TextBlock->SetEnabled(!bIsDisabled);
	}
}

#endif
