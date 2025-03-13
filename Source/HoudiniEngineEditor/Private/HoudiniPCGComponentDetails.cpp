/*
* Copyright (c) <2025> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "DetailCategoryBuilder.h"
#if defined(HOUDINI_USE_PCG)

#include "HoudiniPCGComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"

// Slate UI elements
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// Debugging (for on-screen messages)
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

TSharedRef<IDetailCustomization> UHoudiniPCGComponentDetails::MakeInstance()
{
	return MakeShareable(new UHoudiniPCGComponentDetails);
}

void UHoudiniPCGComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const FName PCGCategoryName("PCG");
	IDetailCategoryBuilder& PCGCategory = DetailBuilder.EditCategory(PCGCategoryName);

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	TArray<UHoudiniPCGComponent*> Components;

	for(TWeakObjectPtr<UObject> Obj : ObjectsBeingCustomized)
	{
		if(Obj.IsValid())
		{
			UHoudiniPCGComponent* Component = Cast<UHoudiniPCGComponent>(Obj.Get());
			if(IsValid(Component))
				Components.Add(Component);
		}
	}



	for(UHoudiniPCGComponent* Component : Components)
	{
		IDetailCategoryBuilder& MyCategory = DetailBuilder.EditCategory("Houdini");

		FDetailWidgetRow& ButtonRow = MyCategory.AddCustomRow(FText::FromString("Custom Button"));
		TSharedRef<SHorizontalBox> ButtonHorizontalBox = SNew(SHorizontalBox);
		TSharedPtr<SButton> ClearCacheButton;
		TSharedPtr<SHorizontalBox> ClearCookBox;

		ButtonHorizontalBox->AddSlot()
		//	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
			//.Padding(2.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SBox)
				//	.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
					[
						SAssignNew(ClearCacheButton, SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText(LOCTEXT("HoudiniPCGComponentDetails", "Clears internal Cookables data."))
							.Text(FText::FromString("Clear HDA Cache Data"))
							.Visibility(EVisibility::Visible)
							.OnClicked_Lambda([Component]()
								{
									//Component->Cookable->Invalidate();
									return FReply::Handled();
								})
					]
			];

		ButtonRow.WholeRowWidget.Widget = ButtonHorizontalBox;
		ButtonRow.IsEnabled(true);
	}

}

UHoudiniPCGComponentDetails::UHoudiniPCGComponentDetails()
{
}

#undef LOCTEXT_NAMESPACE
#endif