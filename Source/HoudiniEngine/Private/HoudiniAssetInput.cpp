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


UHoudiniAssetInput::UHoudiniAssetInput(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	InputObject(nullptr),
	ConnectedAssetId(-1),
	InputIndex(0)
{

}


UHoudiniAssetInput::~UHoudiniAssetInput()
{

}


UHoudiniAssetInput*
UHoudiniAssetInput::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, int32 InInputIndex)
{
	UHoudiniAssetInput* HoudiniAssetInput = nullptr;

	// Get name of this input. For the time being we only support geometry inputs.
	HAPI_StringHandle InputStringHandle;
	if(HAPI_RESULT_SUCCESS != HAPI_GetInputName(InHoudiniAssetComponent->GetAssetId(), InInputIndex, HAPI_INPUT_GEOMETRY, &InputStringHandle))
	{
		return HoudiniAssetInput;
	}

	HoudiniAssetInput = NewObject<UHoudiniAssetInput>(InHoudiniAssetComponent);

	// Set component and other information.
	HoudiniAssetInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInput->InputIndex = InInputIndex;
	HoudiniAssetInput->ConnectedAssetId = -1;

	// Get input string from handle.
	HoudiniAssetInput->SetNameAndLabel(InputStringHandle);

	return HoudiniAssetInput;
}


void
UHoudiniAssetInput::DestroyHoudiniAsset()
{
	if(-1 != ConnectedAssetId)
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(ConnectedAssetId);
		ConnectedAssetId = -1;
	}
}


bool
UHoudiniAssetInput::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// This implementation is not a true parameter. This method should not be called.
	check(false);
	return false;
}


void
UHoudiniAssetInput::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	StaticMeshThumbnailBorder.Reset();
	StaticMeshComboButton.Reset();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(GetParameterLabel())
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	// Create thumbnail for this static mesh.
	TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	TSharedPtr<SHorizontalBox> ButtonBox;

	VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
	[
		SNew(SAssetDropTarget)
		.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this, &UHoudiniAssetInput::OnStaticMeshDraggedOver))
		.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this, &UHoudiniAssetInput::OnStaticMeshDropped))
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
		]
	];

	HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
	[
		SAssignNew(StaticMeshThumbnailBorder, SBorder)
		.Padding(5.0f)
		.BorderImage(TAttribute<const FSlateBrush*>::Create(TAttribute<const FSlateBrush*>::FGetter::CreateUObject(this, &UHoudiniAssetInput::GetStaticMeshThumbnailBorder)))
		.OnMouseDoubleClick(FPointerEventHandler::CreateUObject(this, &UHoudiniAssetInput::OnThumbnailDoubleClick))
		[
			SNew(SBox)
			.WidthOverride(64)
			.HeightOverride(64)
			.ToolTipText(GetParameterLabel())
			[
				StaticMeshThumbnail->MakeThumbnailWidget()
			]
		]
	];

	FString MeshName = TEXT("");
	if(InputObject)
	{
		MeshName = InputObject->GetName();
	}

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
			+SHorizontalBox::Slot()
			[
				SAssignNew(StaticMeshComboButton, SComboButton)
				.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(FOnGetContent::CreateUObject(this, &UHoudiniAssetInput::OnGetStaticMeshMenuContent))
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(MeshName)
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
	Args.Add(TEXT("Asset"), FText::FromString(MeshName));
	FText StaticMeshTooltip = FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

	ButtonBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	[
		PropertyCustomizationHelpers::MakeBrowseButton(
			FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInput::OnStaticMeshBrowse),
			TAttribute<FText>(StaticMeshTooltip))
	];

	ButtonBox->AddSlot()
	.AutoWidth()
	.Padding(2.0f, 0.0f)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("ResetToBase", "Reset to default static mesh"))
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.ContentPadding(0) 
		.Visibility(EVisibility::Visible)
		.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnResetStaticMeshClicked))
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
		]
	];

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


bool
UHoudiniAssetInput::UploadParameterValue()
{
	if(InputObject)
	{
		if(InputObject->IsA(UStaticMesh::StaticClass()))
		{
			// If we have valid static mesh assigned, we need to marshal it into HAPI.
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(InputObject);

			if(!FHoudiniEngineUtils::HapiCreateAndConnectAsset(HoudiniAssetComponent->GetAssetId(), InputIndex, StaticMesh, ConnectedAssetId))
			{
				bChanged = false;
				return false;
			}
		}
		else if(InputObject->IsA(UHoudiniSplineComponent::StaticClass()))
		{
			// If we have spline component input, we need to input that information.
			UHoudiniSplineComponent* HoudiniSplineComponent = Cast<UHoudiniSplineComponent>(InputObject);
		}
	}
	else if(HasConnectedAsset())
	{
		// We do not have an object bound.
		DestroyHoudiniAsset();
	}

	return Super::UploadParameterValue();
}


void
UHoudiniAssetInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	// Serialize input index.
	Ar << InputIndex;

	// We will only serialize static mesh inputs for the moment.
	bool bMeshAssigned = false;
	FString MeshPathName = TEXT("");

	if(Ar.IsSaving())
	{
		UStaticMesh* StaticMeshInput = Cast<UStaticMesh>(InputObject);
		if(StaticMeshInput)
		{
			bMeshAssigned = true;
			MeshPathName = StaticMeshInput->GetPathName();
		}
	}

	Ar << bMeshAssigned;
	if(bMeshAssigned)
	{
		Ar << MeshPathName;
	}

	if(Ar.IsLoading())
	{
		UStaticMesh* StaticMeshInput = nullptr;
		if(bMeshAssigned)
		{
			InputObject = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshPathName, nullptr, LOAD_NoWarn, nullptr));
		}
	}
}


void
UHoudiniAssetInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInput* HoudiniAssetInput = Cast<UHoudiniAssetInput>(InThis);
	if(HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill())
	{
		// Add reference to held object.
		if(HoudiniAssetInput->InputObject)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputObject, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


bool
UHoudiniAssetInput::HasConnectedAsset() const
{
	return -1 != ConnectedAssetId;
}


void
UHoudiniAssetInput::OnStaticMeshDropped(UObject* Object)
{
	if(Object != InputObject)
	{
		MarkPreChanged();
		InputObject = Object;
		MarkChanged();

		HoudiniAssetComponent->UpdateEditorProperties();
	}
}


bool
UHoudiniAssetInput::OnStaticMeshDraggedOver(const UObject* InObject) const
{
	if(InObject && InObject->IsA(UStaticMesh::StaticClass()))
	{
		return true;
	}

	return false;
}


const FSlateBrush*
UHoudiniAssetInput::GetStaticMeshThumbnailBorder() const
{
	if(StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered())
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
}


FReply
UHoudiniAssetInput::OnThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if(InputObject && GEditor)
	{
		GEditor->EditObject(InputObject);
	}

	return FReply::Handled();
}


TSharedRef<SWidget>
UHoudiniAssetInput::OnGetStaticMeshMenuContent()
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UStaticMesh::StaticClass());

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(InputObject), true, &AllowedClasses, OnShouldFilterStaticMesh,
		FOnAssetSelected::CreateUObject(this, &UHoudiniAssetInput::OnStaticMeshSelected), 
		FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInput::CloseStaticMeshComboButton));
}


void
UHoudiniAssetInput::OnStaticMeshSelected(const FAssetData& AssetData)
{
	if(StaticMeshComboButton.IsValid())
	{
		StaticMeshComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnStaticMeshDropped(Object);
	}
}


void
UHoudiniAssetInput::CloseStaticMeshComboButton()
{

}


void
UHoudiniAssetInput::OnStaticMeshBrowse()
{
	if(GEditor && InputObject)
	{
		TArray<UObject*> Objects;
		Objects.Add(InputObject);
		GEditor->SyncBrowserToObjects(Objects);
	}
}


FReply
UHoudiniAssetInput::OnResetStaticMeshClicked()
{
	OnStaticMeshDropped(nullptr);
	return FReply::Handled();
}

