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

			//TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
			TSharedPtr<SBorder> ThumbnailBorder;
			TSharedPtr<SHorizontalBox> ButtonBox;
			TSharedPtr<SComboButton> AssetComboButton;
			TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
			VerticalBox->AddSlot().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				.AutoWidth()
				[
					SAssignNew(ThumbnailBorder, SBorder)
					.Padding(5.0f)
					.BorderImage(this, &FHoudiniAssetComponentDetails::GetThumbnailBorder, StaticMesh)
					.OnMouseDoubleClick(this, &FHoudiniAssetComponentDetails::OnStaticMeshDoubleClick, StaticMesh)
					[
						SNew(SBox)
						.WidthOverride(64)
						.HeightOverride(64)
						//.ToolTipText( this, &SPropertyEditorAsset::OnGetToolTip )
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
					.VAlign(VAlign_Center)
					[
						SAssignNew(ButtonBox, SHorizontalBox)
						//.IsEnabled(this, &SPropertyEditorAsset::CanEdit)
						+SHorizontalBox::Slot()
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
			StaticMeshThumbnailBorders.Add(StaticMesh, ThumbnailBorder);

			Row.ValueWidget.Widget = VerticalBox;
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
FHoudiniAssetComponentDetails::GetThumbnailBorder(UStaticMesh* StaticMesh) const
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


FReply
FHoudiniAssetComponentDetails::OnStaticMeshDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent, UStaticMesh* StaticMesh)
{
	if(StaticMesh && GEditor)
	{
		GEditor->EditObject(StaticMesh);
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

