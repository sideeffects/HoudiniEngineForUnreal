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
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniApi.h"


UHoudiniAssetParameterFolderList::UHoudiniAssetParameterFolderList(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{

}


UHoudiniAssetParameterFolderList::~UHoudiniAssetParameterFolderList()
{

}


UHoudiniAssetParameterFolderList*
UHoudiniAssetParameterFolderList::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
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

	UHoudiniAssetParameterFolderList* HoudiniAssetParameterFolderList =
		NewObject<UHoudiniAssetParameterFolderList>(Outer, UHoudiniAssetParameterFolderList::StaticClass(), NAME_None, 
			RF_Public | RF_Transactional);

	HoudiniAssetParameterFolderList->CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo);
	return HoudiniAssetParameterFolderList;
}


bool
UHoudiniAssetParameterFolderList::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	if(!Super::CreateParameter(InHoudiniAssetComponent, InParentParameter, InNodeId, ParmInfo))
	{
		return false;
	}

	// We can only handle folder and folder list types.
	if(HAPI_PARMTYPE_FOLDERLIST != ParmInfo.type)
	{
		return false;
	}

	// Assign internal Hapi values index.
	SetValuesIndex(ParmInfo.intValuesIndex);

	return true;
}


#if WITH_EDITOR

void
UHoudiniAssetParameterFolderList::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
	];

	for(int32 ParameterIdx = 0; ParameterIdx < ChildParameters.Num(); ++ParameterIdx)
	{
		UHoudiniAssetParameter* HoudiniAssetParameterChild = ChildParameters[ParameterIdx];
		if(HoudiniAssetParameterChild->IsA(UHoudiniAssetParameterFolder::StaticClass()))
		{
			FText ParameterLabelText = FText::FromString(HoudiniAssetParameterChild->GetParameterLabel());

			HorizontalBox->AddSlot().Padding(0, 2, 0, 2)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(ParameterLabelText)
				.ToolTipText(ParameterLabelText)
				.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetParameterFolderList::OnButtonClick, ParameterIdx))
			];
		}
	}

	Super::CreateWidget(DetailCategoryBuilder);

	if(ChildParameters.Num() > 1)
	{
		TSharedPtr<STextBlock> TextBlock;

		DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
		[
			SAssignNew(TextBlock, STextBlock)
			.Text(FText::GetEmpty())
			.ToolTipText(FText::GetEmpty())
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.WrapTextAt(HAPI_UNREAL_DESIRED_ROW_FULL_WIDGET_WIDTH)
		];

		if(TextBlock.IsValid())
		{
			TextBlock->SetEnabled(!bIsDisabled);
		}
	}
}


FReply
UHoudiniAssetParameterFolderList::OnButtonClick(int32 ParameterIdx)
{
	ActiveChildParameter = ParameterIdx;

	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}

	return FReply::Handled();
}

#endif
