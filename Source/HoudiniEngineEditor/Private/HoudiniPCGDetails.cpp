/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HoudiniPCGDetails.h"
#include "HoudiniPCGNode.h"
#include "DetailLayoutBuilder.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE


TSharedRef<IDetailCustomization> UHoudiniPCGSettingsCustomization::MakeInstance()
{
	return MakeShareable(new UHoudiniPCGSettingsCustomization);
}

void UHoudiniPCGSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	TArray<TWeakObjectPtr<UHoudiniPCGSettings>> SettingsBeingCustomized;
	TArray<TWeakObjectPtr<UHoudiniCookable>> CookablesBeingCustomized;


	for (auto Obj : ObjectsBeingCustomized)
	{
		UHoudiniPCGSettings* Settings = Cast<UHoudiniPCGSettings>(Obj.Get());
		if (IsValid(Settings))
		{
			SettingsBeingCustomized.Add(Settings);
			if(IsValid(Settings->ParameterCookable))
			{
				UHoudiniCookable* Cookable = Settings->ParameterCookable->Cookable;

				if(Cookable)
				{
					CookablesBeingCustomized.Add(Cookable);
				}
			}

		}
	}

	for (auto Settings : SettingsBeingCustomized)
	{
		if(!Settings.IsValid())
			continue;


		IDetailCategoryBuilder& SettingsCategory = DetailBuilder.EditCategory(TEXT("HoudiniPCG"));

		SettingsCategory.AddCustomRow(FText::GetEmpty()).ValueContent()
			.MaxDesiredWidth(120.f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Fill)
					[
						SNew(SButton)
							.OnClicked_Lambda([Settings]() -> FReply
								{
									if (Settings.IsValid())
									{
										Settings.Get()->ResetFromHDA();
									}
									return FReply::Handled();
								})
							.ToolTipText(FText::FromString("Resets the node from the HDA, resetting parameters, inputs and outputs."))
							[
								SNew(STextBlock)
									.Font(IDetailLayoutBuilder::GetDetailFont())
									.Text(LOCTEXT("UHoudiniPCGSettingsCustomizationRebuildHDA", "Reset From HDA"))
							]
					]
			];

		if(!IsValid(Settings->ParameterCookable) || !IsValid(Settings->ParameterCookable->Cookable))
			continue;

		UHoudiniCookable* Cookable = Settings->ParameterCookable->Cookable;

		if (!Settings->ParameterCookable->Cookable->GetIsPCG())
		{
			Settings->ParameterCookable->Cookable->SetIsPCG(true);
		}

		TArray<TWeakObjectPtr<UHoudiniCookable>> Cookables;
		Cookables.Add(Cookable);

		EHoudiniDetailsFlags Flags;
		Flags.bAutoBake = false;
		Flags.bBakeButton = false;
		Flags.bDisplayOnOutputLess = true;
		Flags.bAssetOptions = false;
		Flags.bGenerateBar = false;
		Flags.bReplacePreviousBake = false;

		CookableDetails->CreateHoudiniEngineDetails(DetailBuilder, Cookables, FString(), Flags);

		if(Cookable->IsPDGSupported())
		{
			CookableDetails->CreatePDGDetails(DetailBuilder, Cookables);
		}

		CookableDetails->CreateInputDetails(DetailBuilder, Cookables);
		CookableDetails->CreateParameterDetails(DetailBuilder, Cookables);
	}
}

UHoudiniPCGSettingsCustomization::UHoudiniPCGSettingsCustomization()
{
	CookableDetails = MakeShareable(new FHoudiniCookableDetails());
}

#undef LOCTEXT_NAMESPACE 
