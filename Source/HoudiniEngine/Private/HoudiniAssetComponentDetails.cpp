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


uint32
GetTypeHash(TPair<UStaticMesh*, int32> Pair)
{
	return PointerHash(Pair.Key, Pair.Value);
}


float
FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth = 270;


TSharedRef<IDetailCustomization>
FHoudiniAssetComponentDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniAssetComponentDetails);
}


FHoudiniAssetComponentDetails::FHoudiniAssetComponentDetails()
{

}


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

	// Create buttons for actions.
	/*
	DetailBuilder.EditCategory("HoudiniActions", TEXT(""), ECategoryPriority::Important)
		.AddCustomRow(TEXT(""))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0, 2.0f, 0, 0)
			.FillHeight(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(this, &FHoudiniAssetComponentDetails::OnButtonClickedBake)
						.Text(LOCTEXT("BakeHoudiniActor", "Bake"))
						.ToolTipText( LOCTEXT("BakeHoudiniActorToolTip", "Bake selected Houdini Actor into static meshes"))
					]
			]
		];
	*/

	// Create Houdini Asset category.
	DetailBuilder.EditCategory("HoudiniAsset", TEXT(""), ECategoryPriority::Important);

	// Create category for generated static meshes and their materials.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniGeneratedMeshes", TEXT(""), ECategoryPriority::Important);
		CreateStaticMeshAndMaterialWidgets(DetailCategoryBuilder);
	}

	// Create Houdini Generated Static mesh settings category.
	DetailBuilder.EditCategory("HoudiniGeneratedStaticMeshSettings", TEXT(""), ECategoryPriority::Important);

	// Create Houdini Inputs.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniDragAndDropInputs", TEXT(""), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(HoudiniAssetComponent->Inputs); IterInputs; ++IterInputs)
			{
				UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
				HoudiniAssetInput->CreateWidget(DetailCategoryBuilder);
			}
		}
	}

	// Create Houdini Instanced Inputs category.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniDragAndDropInstancedInputs", TEXT(""), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator IterInstancedInputs(HoudiniAssetComponent->InstanceInputs); IterInstancedInputs; ++IterInstancedInputs)
			{
				UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstancedInputs.Value();
				HoudiniAssetInstanceInput->CreateWidget(DetailCategoryBuilder);
			}
		}
	}

	// Create Houdini properties.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniParameters", TEXT(""), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
			{
				UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
				HoudiniAssetParameter->CreateWidget(DetailCategoryBuilder);
			}
		}
	}
}


void
FHoudiniAssetComponentDetails::CreateStaticMeshAndMaterialWidgets(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	StaticMeshThumbnailBorders.Empty();
	MaterialInterfaceComboButtons.Empty();

	for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
	{
		int32 MeshIdx = 0;

		UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
		for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator IterMeshes(HoudiniAssetComponent->StaticMeshComponents); IterMeshes; ++IterMeshes)
		{
			UStaticMesh* StaticMesh = IterMeshes.Key();

			FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

			FString Label = FString::Printf(TEXT("Static Mesh %d"), MeshIdx);
			Row.NameWidget.Widget = SNew(STextBlock)
									.Text(Label)
									.ToolTipText(Label)
									.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

			// Get thumbnail pool for this builder.
			IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
			TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

			// Create thumbnail for this mesh.
			TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(new FAssetThumbnail(StaticMesh, 64, 64, AssetThumbnailPool));

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
					.OnMouseDoubleClick(this, &FHoudiniAssetComponentDetails::OnThumbnailDoubleClick, (UObject*) StaticMesh)
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
							.OnClicked(this, &FHoudiniAssetComponentDetails::OnBakeStaticMesh, StaticMesh, HoudiniAssetComponent)
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
				TSharedPtr<FAssetThumbnail> MaterialInterfaceThumbnail = MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

				VerticalBox->AddSlot().Padding(0, 2)
				[
					SNew(SAssetDropTarget)
					.OnIsAssetAcceptableForDrop( this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceDraggedOver)
					.OnAssetDropped(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceDropped, StaticMesh, MaterialIdx)
					[
						SAssignNew(HorizontalBox, SHorizontalBox)
					]
				];

				HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
				[
					SAssignNew(MaterialThumbnailBorder, SBorder)
					.Padding(5.0f)
					.BorderImage(this, &FHoudiniAssetComponentDetails::GetMaterialInterfaceThumbnailBorder, MaterialInterface)
					.OnMouseDoubleClick(this, &FHoudiniAssetComponentDetails::OnThumbnailDoubleClick, (UObject*) MaterialInterface)
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

				TSharedPtr<SComboButton> AssetComboButton;
				TSharedPtr<SHorizontalBox> ButtonBox;

				HorizontalBox->AddSlot()
				.FillWidth(1.0f)
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					//.VAlign(VAlign_Center)
					//.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ButtonBox, SHorizontalBox)
						//.IsEnabled(this, &FHoudiniAssetComponentDetails::CanEdit)
						+SHorizontalBox::Slot()
						[
							SAssignNew(AssetComboButton, SComboButton)
							//.ToolTipText(this, &FHoudiniAssetComponentDetails::OnGetToolTip )
							.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
							.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
							.OnGetMenuContent(this, &FHoudiniAssetComponentDetails::OnGetMaterialInterfaceMenuContent, MaterialInterface, StaticMesh, MaterialIdx)
							.ContentPadding(2.0f)
							.ButtonContent()
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
								.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
								.Text(MaterialInterface->GetName())
							]
						]
					]/*
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin( 0.0f, 0.0f ))
					[
						SNullWidget::NullWidget
					]*/
				];

				// Create tooltip.
				FFormatNamedArguments Args;
				Args.Add(TEXT("Asset"), FText::FromString(MaterialInterface->GetName()));
				FText MaterialTooltip = FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

				ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateSP(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceBrowse, MaterialInterface),
						TAttribute<FText>(MaterialTooltip))
				];

				ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ResetToBase", "Reset to base material"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0) 
					.Visibility(EVisibility::Visible)
					.OnClicked(this, &FHoudiniAssetComponentDetails::OnResetMaterialInterfaceClicked, StaticMesh, MaterialIdx)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				];

				// Store combo button for this mesh and index.
				TPairInitializer<UStaticMesh*, int32> Pair(StaticMesh, MaterialIdx);
				MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
			}

			Row.ValueWidget.Widget = VerticalBox;
			Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
			
			MeshIdx++;
		}
	}

	DetailCategoryBuilder.AddCustomRow(TEXT(""))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0, 2.0f, 0, 0)
		.FillHeight(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
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
				]
		]
	];
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
FHoudiniAssetComponentDetails::GetMaterialInterfaceThumbnailBorder(UMaterialInterface* MaterialInterface) const
{
	/*
	TSharedPtr<SBorder> ThumbnailBorder = StaticMeshThumbnailBorders[StaticMesh];
	if(ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
	*/

	return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}


FReply
FHoudiniAssetComponentDetails::OnThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent, UObject* Object)
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
	if(StaticMesh)
	{
		UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::BakeStaticMesh(HoudiniAssetComponent, StaticMesh, -1);
		
		// Notify asset registry that we have created assets. This should update the content browser.
		FAssetRegistryModule::AssetCreated(OutStaticMesh);
	}

	return FReply::Handled();
}


FReply
FHoudiniAssetComponentDetails::OnBakeAllStaticMeshes()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		int32 MeshIdx = 0;
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(HoudiniAssetComponent->StaticMeshes); Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::BakeStaticMesh(HoudiniAssetComponent, StaticMesh, MeshIdx);
			MeshIdx++;

			// Notify asset registry that we have created assets. This should update the content browser.
			FAssetRegistryModule::AssetCreated(OutStaticMesh);
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
FHoudiniAssetComponentDetails::OnMaterialInterfaceDropped(UObject* InObject, UStaticMesh* StaticMesh, int32 MaterialIdx)
{
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InObject);
	if(MaterialInterface)
	{
		// Replace material.
		StaticMesh->Materials[MaterialIdx] = MaterialInterface;
	}
}


TSharedRef<SWidget>
FHoudiniAssetComponentDetails::OnGetMaterialInterfaceMenuContent(UMaterialInterface* MaterialInterface, UStaticMesh* StaticMesh, int32 MaterialIdx)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UMaterialInterface::StaticClass());

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(MaterialInterface), true, &AllowedClasses, OnShouldFilterMaterialInterface,
		FOnAssetSelected::CreateSP(this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceSelected, StaticMesh, MaterialIdx), 
		FSimpleDelegate::CreateSP(this, &FHoudiniAssetComponentDetails::CloseMaterialInterfaceComboButton));
}


void
FHoudiniAssetComponentDetails::OnMaterialInterfaceSelected(const FAssetData& AssetData, UStaticMesh* StaticMesh, int32 MaterialIdx)
{
	TPairInitializer<UStaticMesh*, int32> Pair(StaticMesh, MaterialIdx);
	TSharedPtr<SComboButton> AssetComboButton = MaterialInterfaceComboButtons[Pair];
	if(AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnMaterialInterfaceDropped(Object, StaticMesh, MaterialIdx);
	}
}


void
FHoudiniAssetComponentDetails::CloseMaterialInterfaceComboButton()
{

}


void
FHoudiniAssetComponentDetails::OnMaterialInterfaceBrowse(UMaterialInterface* MaterialInterface)
{
	TArray<UObject*> Objects;
	Objects.Add(MaterialInterface);
	GEditor->SyncBrowserToObjects(Objects);
}


FReply
FHoudiniAssetComponentDetails::OnResetMaterialInterfaceClicked(UStaticMesh* StaticMesh, int32 MaterialIdx)
{
	UMaterialInterface* MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
	if(MaterialInterface)
	{
		// Replace material.
		StaticMesh->Materials[MaterialIdx] = MaterialInterface;
	}

	return FReply::Handled();
}

