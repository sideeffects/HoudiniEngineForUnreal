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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniApi.h"
#include "HoudiniAssetLogWidget.h"


uint32
GetTypeHash(TPair<UStaticMesh*, int32> Pair)
{
	return PointerHash(Pair.Key, Pair.Value);
}


TSharedRef<IDetailCustomization>
FHoudiniAssetComponentDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniAssetComponentDetails);
}


FHoudiniAssetComponentDetails::FHoudiniAssetComponentDetails()
{

}


FHoudiniAssetComponentDetails::~FHoudiniAssetComponentDetails()
{

}


/*
void
FHoudiniAssetComponentDetails::CreateSingleStaticMesh()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::BakeSingleStaticMesh(HoudiniAssetComponent, HoudiniAssetComponent->StaticMeshComponents);
		if(OutStaticMesh)
		{
			// Notify asset registry that we have created assets. This should update the content browser.
			FAssetRegistryModule::AssetCreated(OutStaticMesh);
		}
	}
}


FReply
FHoudiniAssetComponentDetails::OnButtonClickedBakeSingle()
{
	CreateSingleStaticMesh();
	return FReply::Handled();
}
*/


void
FHoudiniAssetComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get all components which are being customized.
	TArray<TWeakObjectPtr<UObject> > ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	// See if we need to enable baking option.
	for(int32 i = 0; i < ObjectsCustomized.Num(); ++i)
	{
		if(ObjectsCustomized[i].IsValid())
		{
			UObject* Object = ObjectsCustomized[i].Get();

			if(Object)
			{
				UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(Object);
				HoudiniAssetComponents.Add(HoudiniAssetComponent);
			}
		}
	}

	// Create Houdini Asset category.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder =
			DetailBuilder.EditCategory("HoudiniAsset", FText::GetEmpty(), ECategoryPriority::Important);
		CreateHoudiniAssetWidget(DetailCategoryBuilder);
	}

	// Create category for generated static meshes and their materials.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder =
			DetailBuilder.EditCategory("HoudiniGeneratedMeshes", FText::GetEmpty(), ECategoryPriority::Important);
		CreateStaticMeshAndMaterialWidgets(DetailCategoryBuilder);
	}

	// Create Houdini Generated Static mesh settings category.
	DetailBuilder.EditCategory("HoudiniGeneratedStaticMeshSettings", FText::GetEmpty(), ECategoryPriority::Important);

	// Create Houdini Inputs.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder =
			DetailBuilder.EditCategory("HoudiniInputs", FText::GetEmpty(), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator
			IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TArray<UHoudiniAssetInput*>::TIterator
				IterInputs(HoudiniAssetComponent->Inputs); IterInputs; ++IterInputs)
			{
				UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
				HoudiniAssetInput->CreateWidget(DetailCategoryBuilder);
			}
		}
	}

	// Create Houdini Instanced Inputs category.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniInstancedInputs", FText::GetEmpty(),
			ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator
			IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator
				IterInstancedInputs(HoudiniAssetComponent->InstanceInputs); IterInstancedInputs; ++IterInstancedInputs)
			{
				UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstancedInputs.Value();
				HoudiniAssetInstanceInput->CreateWidget(DetailCategoryBuilder);
			}
		}
	}

	// Create Houdini parameters.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder =
			DetailBuilder.EditCategory("HoudiniParameters", FText::GetEmpty(), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator
			IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TMap<HAPI_ParmId, UHoudiniAssetParameter*>::TIterator
				IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
			{
				UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

				// We want to create root parameters here; they will recursively create child parameters.
				if(!HoudiniAssetParameter->IsChildParameter())
				{
					HoudiniAssetParameter->CreateWidget(DetailCategoryBuilder);
				}
			}
		}
	}
}


void
FHoudiniAssetComponentDetails::CreateStaticMeshAndMaterialWidgets(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	StaticMeshThumbnailBorders.Empty();
	MaterialInterfaceComboButtons.Empty();
	MaterialInterfaceThumbnailBorders.Empty();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	for(TArray<UHoudiniAssetComponent*>::TIterator
		IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
	{
		int32 MeshIdx = 0;
		UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;

		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator
			IterMeshes(HoudiniAssetComponent->StaticMeshes); IterMeshes; ++IterMeshes)
		{
			UStaticMesh* StaticMesh = IterMeshes.Value();
			FHoudiniGeoPartObject& HoudiniGeoPartObject = IterMeshes.Key();

			if(!StaticMesh)
			{
				continue;
			}

			FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

			FString Label = FString::Printf(TEXT("Static Mesh %d"), MeshIdx);
			Row.NameWidget.Widget = SNew(STextBlock)
									.Text(Label)
									.ToolTipText(Label)
									.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

			// Create thumbnail for this mesh.
			TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(new FAssetThumbnail(StaticMesh, 64, 64,
				AssetThumbnailPool));

			TSharedPtr<SBorder> StaticMeshThumbnailBorder;
			TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
			VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				.AutoWidth()
				[
					SAssignNew(StaticMeshThumbnailBorder, SBorder)
					.Padding(5.0f)
					.BorderImage(this, &FHoudiniAssetComponentDetails::GetStaticMeshThumbnailBorder, StaticMesh)
					.OnMouseDoubleClick(this, &FHoudiniAssetComponentDetails::OnThumbnailDoubleClick,
						(UObject*) StaticMesh)
					[
						SNew(SBox)
						.WidthOverride(64)
						.HeightOverride(64)
						.ToolTipText(StaticMesh->GetPathName())
						[
							StaticMeshThumbnail->MakeThumbnailWidget()
						]
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.MaxWidth(80.0f)
						[
							SNew(SButton)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("Bake", "Bake"))
							.OnClicked(this, &FHoudiniAssetComponentDetails::OnBakeStaticMesh, StaticMesh,
								HoudiniAssetComponent)
							.ToolTipText(LOCTEXT("HoudiniStaticMeshBakeButton", "Bake this generated static mesh"))
						]
					]
				]
			];

			// Store thumbnail for this mesh.
			StaticMeshThumbnailBorders.Add(StaticMesh, StaticMeshThumbnailBorder);

			// We need to add material box for each material present in this static mesh.
			TArray<UMaterialInterface*>& StaticMeshMaterials = StaticMesh->Materials;
			for(int32 MaterialIdx = 0; MaterialIdx < StaticMeshMaterials.Num(); ++MaterialIdx)
			{
				UMaterialInterface* MaterialInterface = StaticMeshMaterials[MaterialIdx];
				TSharedPtr<SBorder> MaterialThumbnailBorder;
				TSharedPtr<SHorizontalBox> HorizontalBox = NULL;

				// Create thumbnail for this material.
				TSharedPtr<FAssetThumbnail> MaterialInterfaceThumbnail =
					MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

				VerticalBox->AddSlot().Padding(0, 2)
				[
					SNew(SAssetDropTarget)
					.OnIsAssetAcceptableForDrop(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceDraggedOver)
					.OnAssetDropped(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceDropped,
						StaticMesh, &HoudiniGeoPartObject, MaterialIdx)
					[
						SAssignNew(HorizontalBox, SHorizontalBox)
					]
				];

				HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
				[
					SAssignNew(MaterialThumbnailBorder, SBorder)
					.Padding(5.0f)
					.BorderImage(this, &FHoudiniAssetComponentDetails::GetMaterialInterfaceThumbnailBorder, StaticMesh,
						MaterialIdx)
					.OnMouseDoubleClick(this,
						&FHoudiniAssetComponentDetails::OnThumbnailDoubleClick, (UObject*) MaterialInterface)
					[
						SNew(SBox)
						.WidthOverride(64)
						.HeightOverride(64)
						.ToolTipText(MaterialInterface->GetPathName())
						[
							MaterialInterfaceThumbnail->MakeThumbnailWidget()
						]
					]
				];

				// Store thumbnail for this mesh and material index.
				{
					TPairInitializer<UStaticMesh*, int32> Pair(StaticMesh, MaterialIdx);
					MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
				}

				TSharedPtr<SComboButton> AssetComboButton;
				TSharedPtr<SHorizontalBox> ButtonBox;

				HorizontalBox->AddSlot()
				.FillWidth(1.0f)
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ButtonBox, SHorizontalBox)
						+SHorizontalBox::Slot()
						[
							SAssignNew(AssetComboButton, SComboButton)
							//.ToolTipText(this, &FHoudiniAssetComponentDetails::OnGetToolTip)
							.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
							.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
							.OnGetMenuContent(this, &FHoudiniAssetComponentDetails::OnGetMaterialInterfaceMenuContent,
								MaterialInterface, StaticMesh, &HoudiniGeoPartObject, MaterialIdx)
							.ContentPadding(2.0f)
							.ButtonContent()
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
								.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
								.Text(MaterialInterface->GetName())
							]
						]
					]
				];

				// Create tooltip.
				FFormatNamedArguments Args;
				Args.Add(TEXT("Asset"), FText::FromString(MaterialInterface->GetName()));
				FText MaterialTooltip = FText::Format(
					LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

				ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateSP(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceBrowse,
							MaterialInterface),
						TAttribute<FText>(MaterialTooltip))
				];

				ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ResetToBaseMaterial", "Reset to base material"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.Visibility(EVisibility::Visible)
					.OnClicked(this, &FHoudiniAssetComponentDetails::OnResetMaterialInterfaceClicked,
						StaticMesh, &HoudiniGeoPartObject, MaterialIdx)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				];

				// Store combo button for this mesh and index.
				{
					TPairInitializer<UStaticMesh*, int32> Pair(StaticMesh, MaterialIdx);
					MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
				}
			}

			Row.ValueWidget.Widget = VerticalBox;
			Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

			MeshIdx++;
		}
	}

	TSharedRef<SHorizontalBox> HorizontalButtonBox = SNew(SHorizontalBox);
	DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0, 2.0f, 0, 0)
		.FillHeight(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(HorizontalButtonBox, SHorizontalBox)
		]
	];

	HorizontalButtonBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FHoudiniAssetComponentDetails::OnBakeAllStaticMeshes)
		.Text(LOCTEXT("BakeHoudiniActor", "Bake All"))
		.ToolTipText( LOCTEXT("BakeHoudiniActorToolTip", "Bake all generated static meshes"))
	];
	/*
	HorizontalButtonBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SAssignNew(ButtonRecook, SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FHoudiniAssetComponentDetails::OnRecookAsset)
		.Text(LOCTEXT("RecookHoudiniActor", "Recook"))
		.ToolTipText( LOCTEXT("RecookHoudiniActorToolTip", "Recook Houdini asset"))
	];
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];
		ButtonRecook->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(HoudiniAssetComponent, &UHoudiniAssetComponent::IsNotCookingOrInstantiating)));
	}
	else
	{
		ButtonRecook->SetEnabled(false);
	}
	*/
}


void
FHoudiniAssetComponentDetails::CreateHoudiniAssetWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	// Reset Houdini asset related widgets.
	HoudiniAssetComboButton.Reset();
	HoudiniAssetThumbnailBorder.Reset();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

	FString Label = TEXT("Houdini Asset");
	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(Label)
							.ToolTipText(Label)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	UHoudiniAsset* HoudiniAsset = nullptr;
	UHoudiniAssetComponent* HoudiniAssetComponent = nullptr;
	FString HoudiniAssetPathName = TEXT("");
	FString HoudiniAssetName = TEXT("");

	if(HoudiniAssetComponents.Num() > 0)
	{
		HoudiniAssetComponent = HoudiniAssetComponents[0];
		HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

		if(HoudiniAsset)
		{
			HoudiniAssetPathName = HoudiniAsset->GetPathName();
			HoudiniAssetName = HoudiniAsset->GetName();
		}
	}

	// Create thumbnail for this Houdini asset.
	TSharedPtr<FAssetThumbnail> HoudiniAssetThumbnail =
		MakeShareable(new FAssetThumbnail(HoudiniAsset, 64, 64, AssetThumbnailPool));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	TSharedPtr<SHorizontalBox> ButtonBox;

	TSharedRef<SButton> ButtonRecook = SNew(SButton);
	TSharedRef<SButton> ButtonReset = SNew(SButton);

	VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
	[
		SNew(SAssetDropTarget)
		.OnIsAssetAcceptableForDrop(this, &FHoudiniAssetComponentDetails::OnHoudiniAssetDraggedOver)
		.OnAssetDropped(this, &FHoudiniAssetComponentDetails::OnHoudiniAssetDropped)
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
		]
	];

	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this, &FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingCooking,
			HoudiniAssetComponent)
		.IsChecked(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingCooking, HoudiniAssetComponent)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEnableCookingOnParamChange", "Enable Cooking on Parameter Change"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this,
			&FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingUploadTransform, HoudiniAssetComponent)
		.IsChecked(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingUploadTransform,
			HoudiniAssetComponent)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniUploadTransformsToHoudiniEngine", "Upload Transforms to Houdini Engine"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this,
			&FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingTransformCooking, HoudiniAssetComponent)
		.IsChecked(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingTransformCooking,
			HoudiniAssetComponent)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniTransformChangeTriggersCooks", "Transform Change Triggers Cooks"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	{
		TSharedRef<SHorizontalBox> HorizontalButtonBox = SNew(SHorizontalBox);
		DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0, 2.0f, 0, 0)
			.FillHeight(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(HorizontalButtonBox, SHorizontalBox)
			]
		];

		HorizontalButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(ButtonRecook, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FHoudiniAssetComponentDetails::OnRecookAsset)
			.Text(LOCTEXT("RecookHoudiniActor", "Recook Asset"))
			.ToolTipText( LOCTEXT("RecookHoudiniActorToolTip", "Recook Houdini asset"))
		];

		HorizontalButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(ButtonRecook, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FHoudiniAssetComponentDetails::OnRebuildAsset)
			.Text(LOCTEXT("RebuildHoudiniActor", "Rebuild Asset"))
			.ToolTipText( LOCTEXT("RebuildHoudiniActorToolTip", "Rebuild Houdini asset"))
		];

		HorizontalButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(ButtonReset, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FHoudiniAssetComponentDetails::OnResetAsset)
			.Text(LOCTEXT("ResetHoudiniActor", "Reset Parameters"))
			.ToolTipText( LOCTEXT("ResetHoudiniActorToolTip", "Reset Parameters"))
		];
	}

	{
		TSharedRef<SHorizontalBox> HorizontalButtonBox = SNew(SHorizontalBox);
		DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0, 2.0f, 0, 0)
			.FillHeight(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(HorizontalButtonBox, SHorizontalBox)
			]
		];

		HorizontalButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(ButtonReset, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FHoudiniAssetComponentDetails::OnFetchCookLog)
			.Text(LOCTEXT("FetchCookLogHoudiniActor", "Fetch Cook Log"))
			.ToolTipText( LOCTEXT("FetchCookLogHoudiniActorToolTip", "Fetch Cook Log"))
		];

		HorizontalButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(ButtonReset, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FHoudiniAssetComponentDetails::OnFetchAssetHelp, HoudiniAssetComponent)
			.Text(LOCTEXT("FetchAssetHelpHoudiniActor", "Asset Help"))
			.ToolTipText( LOCTEXT("FetchAssetHelpHoudiniActorToolTip", "Asset Help"))
		];

		HorizontalButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(ButtonReset, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FHoudiniAssetComponentDetails::OnBakeBlueprint)
			.Text(LOCTEXT("BakeBlueprintHoudiniActor", "Bake Blueprint"))
			.ToolTipText( LOCTEXT("BakeBlueprintHoudiniActorToolTip", "Bake Blueprint"))
		];
	}

	HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
	[
		SAssignNew(HoudiniAssetThumbnailBorder, SBorder)
		.Padding(5.0f)
		.BorderImage(this, &FHoudiniAssetComponentDetails::GetHoudiniAssetThumbnailBorder)
		[
			SNew(SBox)
			.WidthOverride(64)
			.HeightOverride(64)
			.ToolTipText(HoudiniAssetPathName)
			[
				HoudiniAssetThumbnail->MakeThumbnailWidget()
			]
		]
	];

	HorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SAssignNew(HoudiniAssetComboButton, SComboButton)
				.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &FHoudiniAssetComponentDetails::OnGetHoudiniAssetMenuContent)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(HoudiniAssetName)
				]
			]
		]
	];

	ButtonBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeBrowseButton(
			FSimpleDelegate::CreateSP(this, &FHoudiniAssetComponentDetails::OnHoudiniAssetBrowse),
			TAttribute<FText>(FText::FromString(HoudiniAssetName)))
	];

	ButtonBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("ResetToBaseHoudiniAsset", "Reset Houdini Asset"))
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.ContentPadding(0)
		.Visibility(EVisibility::Visible)
		.OnClicked(this, &FHoudiniAssetComponentDetails::OnResetHoudiniAssetClicked)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
		]
	];

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}


const FSlateBrush*
FHoudiniAssetComponentDetails::GetStaticMeshThumbnailBorder(UStaticMesh* StaticMesh) const
{
	TSharedPtr<SBorder> ThumbnailBorder = StaticMeshThumbnailBorders[StaticMesh];
	if(ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
}


const FSlateBrush*
FHoudiniAssetComponentDetails::GetMaterialInterfaceThumbnailBorder(UStaticMesh* StaticMesh, int32 MaterialIdx) const
{
	TPairInitializer<UStaticMesh*, int32> Pair(StaticMesh, MaterialIdx);
	TSharedPtr<SBorder> ThumbnailBorder = MaterialInterfaceThumbnailBorders[Pair];

	if(ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
}


FReply
FHoudiniAssetComponentDetails::OnThumbnailDoubleClick(const FGeometry& InMyGeometry,
	const FPointerEvent& InMouseEvent, UObject* Object)
{
	if(Object && GEditor)
	{
		GEditor->EditObject(Object);
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnBakeStaticMesh(UStaticMesh* StaticMesh, UHoudiniAssetComponent* HoudiniAssetComponent)
{
	if(HoudiniAssetComponent && StaticMesh)
	{
		// We need to locate corresponding geo part object in component.
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = HoudiniAssetComponent->LocateGeoPartObject(StaticMesh);

		// Bake static mesh.
		UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::BakeStaticMesh(HoudiniAssetComponent, HoudiniGeoPartObject,
			StaticMesh);

		if(OutStaticMesh)
		{
			// Notify asset registry that we have created assets. This should update the content browser.
			FAssetRegistryModule::AssetCreated(OutStaticMesh);
			//OutStaticMesh->MakePackageDirty();
		}
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnBakeAllStaticMeshes()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator
			Iter(HoudiniAssetComponent->StaticMeshes); Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::BakeStaticMesh(HoudiniAssetComponent,
				HoudiniGeoPartObject, StaticMesh);

			if(OutStaticMesh)
			{
				// Notify asset registry that we have created assets. This should update the content browser.
				FAssetRegistryModule::AssetCreated(OutStaticMesh);
				//OutStaticMesh->MakePackageDirty();
			}
		}
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnRecookAsset()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		// If component is not cooking or instancing, we can start recook.
		if(HoudiniAssetComponent->IsNotCookingOrInstantiating())
		{
			HoudiniAssetComponent->StartTaskAssetCookingManual();
		}
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnRebuildAsset()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		// If component is not cooking or instancing, we can start recook.
		if(HoudiniAssetComponent->IsNotCookingOrInstantiating())
		{
			HoudiniAssetComponent->StartTaskAssetRebuildManual();
		}
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnResetAsset()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		// If component is not cooking or instancing, we can start reset.
		if(HoudiniAssetComponent->IsNotCookingOrInstantiating())
		{
			HoudiniAssetComponent->StartTaskAssetResetManual();
		}
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnBakeBlueprint()
{
	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnFetchCookLog()
{
	TSharedPtr<SWindow> ParentWindow;

	// Get fetch cook status.
	FString CookLogString = FHoudiniEngineUtils::GetCookResult();

	// Check if the main frame is loaded. When using the old main frame it may not be.
	if(FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if(ParentWindow.IsValid())
	{
		TSharedPtr<SHoudiniAssetLogWidget> HoudiniAssetCookLog;

		TSharedRef<SWindow> Window = SNew(SWindow)
									.Title(LOCTEXT("WindowTitle", "Houdini Cook Log"))
									.ClientSize(FVector2D(640, 480));

		Window->SetContent(SAssignNew(HoudiniAssetCookLog, SHoudiniAssetLogWidget).LogText(CookLogString)
			.WidgetWindow(Window));

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnFetchAssetHelp(UHoudiniAssetComponent* HoudiniAssetComponent)
{
	if(HoudiniAssetComponent)
	{
		HAPI_AssetInfo AssetInfo;
		HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();

		if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
		{
			if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(nullptr, AssetId, &AssetInfo))
			{
				FString HelpLogString;
				if(FHoudiniEngineUtils::GetHoudiniString(AssetInfo.helpTextSH, HelpLogString))
				{
					TSharedPtr<SWindow> ParentWindow;

					// Check if the main frame is loaded. When using the old main frame it may not be.
					if(FModuleManager::Get().IsModuleLoaded("MainFrame"))
					{
						IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
						ParentWindow = MainFrame.GetParentWindow();
					}

					if(ParentWindow.IsValid())
					{
						TSharedPtr<SHoudiniAssetLogWidget> HoudiniAssetHelpLog;

						TSharedRef<SWindow> Window = SNew(SWindow)
													.Title(LOCTEXT("WindowTitle", "Houdini Asset Help"))
													.ClientSize(FVector2D(640, 480));

						Window->SetContent(SAssignNew(HoudiniAssetHelpLog, SHoudiniAssetLogWidget).LogText(HelpLogString)
							.WidgetWindow(Window));

						FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
					}
				}
			}
		}
	}

	return FReply::Handled();
}


bool
FHoudiniAssetComponentDetails::OnMaterialInterfaceDraggedOver(const UObject* InObject) const
{
	return (InObject && InObject->IsA(UMaterialInterface::StaticClass()));
}


void
FHoudiniAssetComponentDetails::OnMaterialInterfaceDropped(UObject* InObject, UStaticMesh* StaticMesh,
	FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx)
{
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InObject);
	if(MaterialInterface)
	{
		// Replace material on static mesh.
		StaticMesh->Materials[MaterialIdx] = MaterialInterface;

		// Replace material on component using this static mesh.
		for(TArray<UHoudiniAssetComponent*>::TIterator
			IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			if(HoudiniAssetComponent)
			{
				UStaticMeshComponent* StaticMeshComponent = HoudiniAssetComponent->LocateStaticMeshComponent(StaticMesh);
				if(StaticMeshComponent)
				{
					StaticMeshComponent->SetMaterial(MaterialIdx, MaterialInterface);
				}

				// Update instanced as well.
				HoudiniAssetComponent->UpdateInstancedStaticMeshComponentMaterial(StaticMesh, MaterialIdx,
					MaterialInterface);
			}
		}

		// Mark geo part object - material has been replaced.
		HoudiniGeoPartObject->SetUnrealMaterialAssigned();

		// We need to update editor to reflect changes.
		if(HoudiniAssetComponents.Num() > 0)
		{
			HoudiniAssetComponents[0]->UpdateEditorProperties(false);
		}
	}
}


TSharedRef<SWidget>
FHoudiniAssetComponentDetails::OnGetMaterialInterfaceMenuContent(UMaterialInterface* MaterialInterface,
	UStaticMesh* StaticMesh, FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UMaterialInterface::StaticClass());

	TArray<UFactory*> NewAssetFactories;

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(MaterialInterface), true, AllowedClasses,
		NewAssetFactories, OnShouldFilterMaterialInterface,
		FOnAssetSelected::CreateSP(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceSelected,
			StaticMesh, HoudiniGeoPartObject, MaterialIdx),
		FSimpleDelegate::CreateSP(this, &FHoudiniAssetComponentDetails::CloseMaterialInterfaceComboButton));
}


void
FHoudiniAssetComponentDetails::OnMaterialInterfaceSelected(const FAssetData& AssetData, UStaticMesh* StaticMesh,
	FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx)
{
	TPairInitializer<UStaticMesh*, int32> Pair(StaticMesh, MaterialIdx);
	TSharedPtr<SComboButton> AssetComboButton = MaterialInterfaceComboButtons[Pair];
	if(AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnMaterialInterfaceDropped(Object, StaticMesh, HoudiniGeoPartObject, MaterialIdx);
	}
}


void
FHoudiniAssetComponentDetails::CloseMaterialInterfaceComboButton()
{

}


void
FHoudiniAssetComponentDetails::OnMaterialInterfaceBrowse(UMaterialInterface* MaterialInterface)
{
	if(GEditor)
	{
		TArray<UObject*> Objects;
		Objects.Add(MaterialInterface);
		GEditor->SyncBrowserToObjects(Objects);
	}
}


FReply
FHoudiniAssetComponentDetails::OnResetMaterialInterfaceClicked(UStaticMesh* StaticMesh,
	FHoudiniGeoPartObject* HoudiniGeoPartObject, int32 MaterialIdx)
{
	if(HoudiniGeoPartObject->HasNativeHoudiniMaterial())
	{
		// Geo part does have native Houdini material.

		HoudiniGeoPartObject->ResetUnrealMaterialAssigned();
		OnRecookAsset();
	}
	else
	{
		// Geo part does not have native Houdini material, we just use default Unreal material in this case.

		UMaterialInterface* MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		if(MaterialInterface)
		{
			// Replace material with default.
			OnMaterialInterfaceDropped(MaterialInterface, StaticMesh, HoudiniGeoPartObject, MaterialIdx);
		}
	}

	return FReply::Handled();
}


void
FHoudiniAssetComponentDetails::OnHoudiniAssetDropped(UObject* InObject)
{
	if(InObject)
	{
		UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(InObject);
		if(HoudiniAsset && HoudiniAssetComponents.Num() > 0)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];
			HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);
		}
	}
}


bool
FHoudiniAssetComponentDetails::OnHoudiniAssetDraggedOver(const UObject* InObject) const
{
	return (InObject && InObject->IsA(UHoudiniAsset::StaticClass()));
}



const FSlateBrush*
FHoudiniAssetComponentDetails::GetHoudiniAssetThumbnailBorder() const
{
	if(HoudiniAssetThumbnailBorder.IsValid() && HoudiniAssetThumbnailBorder->IsHovered())
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
}


TSharedRef<SWidget>
FHoudiniAssetComponentDetails::OnGetHoudiniAssetMenuContent()
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UHoudiniAsset::StaticClass());

	TArray<UFactory*> NewAssetFactories;

	UHoudiniAsset* HoudiniAsset = nullptr;
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];
		HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	}

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(HoudiniAsset), true,
		AllowedClasses, NewAssetFactories, OnShouldFilterHoudiniAsset,
		FOnAssetSelected::CreateSP(this, &FHoudiniAssetComponentDetails::OnHoudiniAssetSelected),
		FSimpleDelegate::CreateSP(this, &FHoudiniAssetComponentDetails::CloseHoudiniAssetComboButton));
}


void
FHoudiniAssetComponentDetails::OnHoudiniAssetSelected(const FAssetData& AssetData)
{
	if(HoudiniAssetComboButton.IsValid())
	{
		HoudiniAssetComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnHoudiniAssetDropped(Object);
	}
}


void
FHoudiniAssetComponentDetails::CloseHoudiniAssetComboButton()
{

}


void
FHoudiniAssetComponentDetails::OnHoudiniAssetBrowse()
{
	if(GEditor)
	{
		UHoudiniAsset* HoudiniAsset = nullptr;
		if(HoudiniAssetComponents.Num() > 0)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];
			HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

			if(HoudiniAsset)
			{
				TArray<UObject*> Objects;
				Objects.Add(HoudiniAsset);
				GEditor->SyncBrowserToObjects(Objects);
			}
		}
	}
}


FReply
FHoudiniAssetComponentDetails::OnResetHoudiniAssetClicked()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];
		HoudiniAssetComponent->SetHoudiniAsset(nullptr);
	}

	return FReply::Handled();
}


ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingCooking(UHoudiniAssetComponent* HoudiniAssetComponent) const
{
	if(HoudiniAssetComponent && HoudiniAssetComponent->bEnableCooking)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingUploadTransform(UHoudiniAssetComponent* HoudiniAssetComponent) const
{
	if(HoudiniAssetComponent && HoudiniAssetComponent->bUploadTransformsToHoudiniEngine)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingTransformCooking(UHoudiniAssetComponent* HoudiniAssetComponent) const
{
	if(HoudiniAssetComponent && HoudiniAssetComponent->bTransformChangeTriggersCooks)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingCooking(ECheckBoxState NewState,
	UHoudiniAssetComponent* HoudiniAssetComponent)
{
	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->bEnableCooking = (ECheckBoxState::Checked == NewState);
	}
}


void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingUploadTransform(ECheckBoxState NewState,
	UHoudiniAssetComponent* HoudiniAssetComponent)
{
	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->bUploadTransformsToHoudiniEngine = (ECheckBoxState::Checked == NewState);
	}
}


void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingTransformCooking(ECheckBoxState NewState,
	UHoudiniAssetComponent* HoudiniAssetComponent)
{
	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->bTransformChangeTriggersCooks = (ECheckBoxState::Checked == NewState);
	}
}
