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
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetInput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetActor.h"
#include "HoudiniApi.h"
#include "HoudiniAssetParameterVersion.h"
#include "HoudiniEngineString.h"


UHoudiniAssetInput::UHoudiniAssetInput(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	InputObject(nullptr),
	InputCurve(nullptr),
	InputAssetComponent(nullptr),
	InputLandscapeProxy(nullptr),
	ConnectedAssetId(-1),
	InputIndex(0),
	ChoiceIndex(EHoudiniAssetInputType::GeometryInput),
	bStaticMeshChanged(false),
	bSwitchedToCurve(false),
	bLoadedParameter(false),
	bInputAssetConnectedInHoudini(false),
	bLandscapeInputSelectionOnly(false),
	bLandscapeExportCurves(false),
	bLandscapeExportFullGeometry(false),
	bLandscapeExportMaterials(true),
	bLandscapeExportLighting(false),
	bLandscapeExportNormalizedUVs(false),
	bLandscapeExportTileUVs(false)
{
	ChoiceStringValue = TEXT("");
}


UHoudiniAssetInput::~UHoudiniAssetInput()
{

}


UHoudiniAssetInput*
UHoudiniAssetInput::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, int32 InInputIndex)
{
	UHoudiniAssetInput* HoudiniAssetInput = nullptr;

	// Get name of this input.
	HAPI_StringHandle InputStringHandle;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetInputName(FHoudiniEngine::Get().GetSession(),
		InHoudiniAssetComponent->GetAssetId(), InInputIndex, HAPI_INPUT_GEOMETRY, &InputStringHandle))
	{
		return HoudiniAssetInput;
	}

	HoudiniAssetInput = NewObject<UHoudiniAssetInput>(InHoudiniAssetComponent,
		UHoudiniAssetInput::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	// Set component and other information.
	HoudiniAssetInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInput->InputIndex = InInputIndex;

	// Get input string from handle.
	HoudiniAssetInput->SetNameAndLabel(InputStringHandle);

	// By default geometry input is chosen.
	HoudiniAssetInput->ChoiceIndex = EHoudiniAssetInputType::GeometryInput;

	// Create necessary widget resources.
	HoudiniAssetInput->CreateWidgetResources();

	return HoudiniAssetInput;
}


void
UHoudiniAssetInput::CreateWidgetResources()
{
	ChoiceStringValue = TEXT("");
	StringChoiceLabels.Empty();

	{
		FString* ChoiceLabel = new FString(TEXT("Geometry Input"));
		StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));

		if(EHoudiniAssetInputType::GeometryInput == ChoiceIndex)
		{
			ChoiceStringValue = *ChoiceLabel;
		}
	}
	{
		FString* ChoiceLabel = new FString(TEXT("Asset Input"));
		StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));

		if(EHoudiniAssetInputType::AssetInput == ChoiceIndex)
		{
			ChoiceStringValue = *ChoiceLabel;
		}
	}
	{
		FString* ChoiceLabel = new FString(TEXT("Curve Input"));
		StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));

		if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
		{
			ChoiceStringValue = *ChoiceLabel;
		}
	}
	{
		FString* ChoiceLabel = new FString(TEXT("Landscape Input"));
		StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));

		if(EHoudiniAssetInputType::LandscapeInput == ChoiceIndex)
		{
			ChoiceStringValue = *ChoiceLabel;
		}
	}
}


void
UHoudiniAssetInput::DisconnectAndDestroyInputAsset()
{
	if(EHoudiniAssetInputType::AssetInput == ChoiceIndex)
	{
		if(InputAssetComponent)
		{
			InputAssetComponent->RemoveDownstreamAsset(HoudiniAssetComponent, InputIndex);
		}

		InputAssetComponent = nullptr;
		ConnectedAssetId = -1;
	}
	else
	{
		if(HoudiniAssetComponent)
		{
			HAPI_AssetId HostAssetId = HoudiniAssetComponent->GetAssetId();
			if(FHoudiniEngineUtils::IsValidAssetId(HostAssetId))
			{
				FHoudiniEngineUtils::HapiDisconnectAsset(HostAssetId, InputIndex);
			}
		}

		if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId))
		{
			FHoudiniEngineUtils::DestroyHoudiniAsset(ConnectedAssetId);
			ConnectedAssetId = -1;
		}
	}
}


bool
UHoudiniAssetInput::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// This implementation is not a true parameter. This method should not be called.
	check(false);
	return false;
}


#if WITH_EDITOR

void
UHoudiniAssetInput::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	StaticMeshThumbnailBorder.Reset();
	StaticMeshComboButton.Reset();
	InputTypeComboBox.Reset();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
	FText ParameterLabelText = FText::FromString(GetParameterLabel());

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(ParameterLabelText)
							.ToolTipText(ParameterLabelText)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	// Create thumbnail for this static mesh.
	TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(
		new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	TSharedPtr<SHorizontalBox> ButtonBox;

	if(StringChoiceLabels.Num() > 0)
	{
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(InputTypeComboBox, SComboBox<TSharedPtr<FString> >)
			.OptionsSource(&StringChoiceLabels)
			.InitiallySelectedItem(StringChoiceLabels[ChoiceIndex])
			.OnGenerateWidget(SComboBox<TSharedPtr<FString> >::FOnGenerateWidget::CreateUObject(
				this, &UHoudiniAssetInput::CreateChoiceEntryWidget))
			.OnSelectionChanged(SComboBox<TSharedPtr<FString> >::FOnSelectionChanged::CreateUObject(
				this, &UHoudiniAssetInput::OnChoiceChange))
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(
					this, &UHoudiniAssetInput::HandleChoiceContentText)))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	}

	if(EHoudiniAssetInputType::GeometryInput == ChoiceIndex)
	{
		VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
		[
			SNew(SAssetDropTarget)
			.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(
				this, &UHoudiniAssetInput::OnStaticMeshDraggedOver))
			.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(
				this, &UHoudiniAssetInput::OnStaticMeshDropped))
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
		[
			SAssignNew(StaticMeshThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateUObject(
					this, &UHoudiniAssetInput::GetStaticMeshThumbnailBorder)))
			.OnMouseDoubleClick(FPointerEventHandler::CreateUObject(this, &UHoudiniAssetInput::OnThumbnailDoubleClick))
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(ParameterLabelText)
				[
					StaticMeshThumbnail->MakeThumbnailWidget()
				]
			]
		];

		FText MeshNameText = FText::GetEmpty();
		if(InputObject)
		{
			MeshNameText = FText::FromString(InputObject->GetName());
		}

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
					SAssignNew(StaticMeshComboButton, SComboButton)
					.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
					.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
					.OnGetMenuContent(FOnGetContent::CreateUObject(
						this, &UHoudiniAssetInput::OnGetStaticMeshMenuContent))
					.ContentPadding(2.0f)
					.ButtonContent()
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
						.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
						.Text(MeshNameText)
					]
				]
			]
		];

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), MeshNameText);
		FText StaticMeshTooltip = FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser",
			"Browse to '{Asset}' in Content Browser"), Args);

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
	}
	else if(EHoudiniAssetInputType::AssetInput == ChoiceIndex)
	{
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			PropertyCustomizationHelpers::MakeActorPickerWithMenu(
				nullptr,
				true,
				FOnShouldFilterActor::CreateUObject(this, &UHoudiniAssetInput::OnInputActorFilter),
				FOnActorSelected::CreateUObject(this, &UHoudiniAssetInput::OnInputActorSelected),
				FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInput::OnInputActorCloseComboButton),
				FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInput::OnInputActorUse)
			)
		];
	}
	else if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
	{
		// Go through all input curve parameters and build their widgets recursively.
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator
			IterParams(InputCurveParameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			if(HoudiniAssetParameter)
			{
				HoudiniAssetParameter->CreateWidget(VerticalBox);
			}
		}
	}
	else if(EHoudiniAssetInputType::LandscapeInput == ChoiceIndex)
	{
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			PropertyCustomizationHelpers::MakeActorPickerWithMenu(
				nullptr,
				true,
				FOnShouldFilterActor::CreateUObject(this, &UHoudiniAssetInput::OnLandscapeActorFilter),
				FOnActorSelected::CreateUObject(this, &UHoudiniAssetInput::OnLandscapeActorSelected),
				FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInput::OnLandscapeActorCloseComboButton),
				FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInput::OnLandscapeActorUse)
			)
		];

		{
			TSharedPtr<SCheckBox> CheckBoxExportSelected;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportSelected, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeSelectedCheckbox", "Export Selected Landscape Only"))
					.ToolTipText(LOCTEXT("LandscapeSelectedCheckbox", "Export Selected Landscape Only"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportOnlySelected)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportOnlySelected))
			];
		}

		{
			TSharedPtr<SCheckBox> CheckBoxExportFullGeometry;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportFullGeometry, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeFullGeometryCheckbox", "Export Full Landscape Geometry"))
					.ToolTipText(LOCTEXT("LandscapeFullGeometryCheckbox", "Export Full Landscape Geometry"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportFullGeometry)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportFullGeometry))
			];
		}

		{
			TSharedPtr<SCheckBox> CheckBoxExportMaterials;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportMaterials, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeMaterialsCheckbox", "Export Landscape Materials"))
					.ToolTipText(LOCTEXT("LandscapeMaterialsCheckbox", "Export Landscape Materials"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportMaterials)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportMaterials))
			];
		}

		{
			TSharedPtr<SCheckBox> CheckBoxExportTileUVs;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportTileUVs, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeTileUVsCheckbox", "Export Landscape Tile UVs"))
					.ToolTipText(LOCTEXT("LandscapeTileUVsCheckbox", "Export Landscape Tile UVs"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportTileUVs)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportTileUVs))
			];
		}

		{
			TSharedPtr<SCheckBox> CheckBoxExportNormalizedUVs;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportNormalizedUVs, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs"))
					.ToolTipText(LOCTEXT("LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportNormalizedUVs)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportNormalizedUVs))
			];
		}

		{
			TSharedPtr<SCheckBox> CheckBoxExportLighting;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportLighting, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeLightingCheckbox", "Export Landscape Lighting"))
					.ToolTipText(LOCTEXT("LandscapeLightingCheckbox", "Export Landscape Lighting"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportLighting)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportLighting))
			];
		}

		{
			TSharedPtr<SCheckBox> CheckBoxExportCurves;

			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportCurves, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeCurvesCheckbox", "Export Landscape Curves"))
					.ToolTipText(LOCTEXT("LandscapeCurvesCheckbox", "Export Landscape Curves"))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
						&UHoudiniAssetInput::IsCheckedExportCurves)))
				.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInput::CheckStateChangedExportCurves))
			];

			// Disable curves until we have them implemented.
			if(CheckBoxExportCurves.IsValid())
			{
				CheckBoxExportCurves->SetEnabled(false);
			}
		}

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(1, 2, 4, 2)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("LandscapeInputRecommit", "Recommit Landscape"))
				.ToolTipText(LOCTEXT("LandscapeInputRecommit", "Recommit Landscape"))
				.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnButtonClickRecommit))
			]
		];
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

#endif


bool
UHoudiniAssetInput::UploadParameterValue()
{
	HAPI_AssetId HostAssetId = HoudiniAssetComponent->GetAssetId();

	switch(ChoiceIndex)
	{
		case EHoudiniAssetInputType::GeometryInput:
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(InputObject);
			if(StaticMesh)
			{
				if(bStaticMeshChanged || bLoadedParameter)
				{
					// Disconnect and destroy currently connected asset, if there's one.
					DisconnectAndDestroyInputAsset();

					// Connect input and create connected asset. Will return by reference.
					if(!FHoudiniEngineUtils::HapiCreateAndConnectAsset(HostAssetId, InputIndex, StaticMesh,
						ConnectedAssetId))
					{
						bChanged = false;
						ConnectedAssetId = -1;
						return false;
					}

					bStaticMeshChanged = false;
				}
			}
			else
			{
				// Either mesh was reset or null mesh has been assigned.
				DisconnectAndDestroyInputAsset();
			}

			break;
		}
	
		case EHoudiniAssetInputType::AssetInput:
		{
			// Process connected asset.
			if(InputAssetComponent && FHoudiniEngineUtils::IsValidAssetId(InputAssetComponent->GetAssetId())
				&& !bInputAssetConnectedInHoudini)
			{
				ConnectInputAssetActor();
			}
			else if(bInputAssetConnectedInHoudini && !InputAssetComponent)
			{
				DisconnectInputAssetActor();
			}
			else
			{
				bChanged = false;
				return false;
			}

			break;
		}

		case EHoudiniAssetInputType::CurveInput:
		{
			// If we have no curve asset, create it.
			if(!FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId))
			{
				if(!FHoudiniEngineUtils::HapiCreateCurve(ConnectedAssetId))
				{
					bChanged = false;
					return false;
				}

				// Connect asset.
				FHoudiniEngineUtils::HapiConnectAsset(ConnectedAssetId, 0, HostAssetId, InputIndex);
			}

			if(bLoadedParameter)
			{
				HAPI_AssetInfo CurveAssetInfo;
				FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &CurveAssetInfo);

				// If we just loaded our curve, we need to set parameters.
				for(TMap<FString, UHoudiniAssetParameter*>::TIterator
					IterParams(InputCurveParameters); IterParams; ++IterParams)
				{
					UHoudiniAssetParameter* Parameter = IterParams.Value();
					if(Parameter)
					{
						// We need to update node id for loaded parameters.
						Parameter->SetNodeId(CurveAssetInfo.nodeId);

						// Upload parameter value.
						Parameter->UploadParameterValue();
					}
				}
			}

			// Also upload points.
			HAPI_NodeId NodeId = -1;
			if(FHoudiniEngineUtils::HapiGetNodeId(ConnectedAssetId, 0, 0, NodeId))
			{
				const TArray<FVector>& CurvePoints = InputCurve->GetCurvePoints();

				FString PositionString = TEXT("");
				FHoudiniEngineUtils::CreatePositionsString(CurvePoints, PositionString);

				// Get param id.
				HAPI_ParmId ParmId = -1;
				if(HAPI_RESULT_SUCCESS == 
					FHoudiniApi::GetParmIdFromName(FHoudiniEngine::Get().GetSession(), NodeId,
						HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId))
				{
					std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
					FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), NodeId,
						ConvertedString.c_str(), ParmId, 0);
				}
			}

			// Cook the spline asset.
			FHoudiniApi::CookAsset(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, nullptr);

			// We need to update the curve.
			UpdateInputCurve();

			bSwitchedToCurve = false;

			break;
		}

		case EHoudiniAssetInputType::LandscapeInput:
		{
			if(InputLandscapeProxy)
			{
				// Disconnect and destroy currently connected asset, if there's one.
				DisconnectAndDestroyInputAsset();

				// Connect input and create connected asset. Will return by reference.
				if(!FHoudiniEngineUtils::HapiCreateAndConnectAsset(HostAssetId, InputIndex, InputLandscapeProxy,
					ConnectedAssetId, bLandscapeInputSelectionOnly, bLandscapeExportCurves,
					bLandscapeExportMaterials, bLandscapeExportFullGeometry, bLandscapeExportLighting,
					bLandscapeExportNormalizedUVs, bLandscapeExportTileUVs))
				{
					bChanged = false;
					ConnectedAssetId = -1;
					return false;
				}
			}
			else
			{
				// Either landscape was reset or null landscape has been assigned.
				DisconnectAndDestroyInputAsset();
			}

			break;
		}

		default:
		{
			check(0);
		}
	}


	bLoadedParameter = false;
	return Super::UploadParameterValue();
}


void
UHoudiniAssetInput::BeginDestroy()
{
	Super::BeginDestroy();

	// Destroy anything curve related.
	DestroyInputCurve();

	// Disconnect and destroy the asset we may have connected.
	DisconnectAndDestroyInputAsset();
}


void
UHoudiniAssetInput::PostLoad()
{
	Super::PostLoad();

	// Generate widget related resources.
	CreateWidgetResources();

	// Patch input curve parameter links.
	for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(InputCurveParameters); IterParams; ++IterParams)
	{
		FString ParameterKey = IterParams.Key();
		UHoudiniAssetParameter* Parameter = IterParams.Value();

		if(Parameter)
		{
			Parameter->SetHoudiniAssetComponent(nullptr);
			Parameter->SetParentParameter(this);
		}
	}

	// Set input callback object for this curve.
	if(InputCurve)
	{
		InputCurve->SetHoudiniAssetInput(this);
		InputCurve->AttachTo(HoudiniAssetComponent, NAME_None, EAttachLocation::KeepRelativeOffset);
	}
}


void
UHoudiniAssetInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	// Serialize current choice selection.
	SerializeEnumeration<EHoudiniAssetInputType::Enum>(Ar, ChoiceIndex);
	Ar << ChoiceStringValue;

	// We need these temporary variables for undo state tracking.
	bool bLocalInputAssetConnectedInHoudini = bInputAssetConnectedInHoudini;
	UHoudiniAssetComponent* LocalInputAssetComponent = InputAssetComponent;

	Ar << HoudiniAssetInputFlagsPacked;

	// Serialize input index.
	Ar << InputIndex;

	// Serialize input object (if it's assigned).
	Ar << InputObject;

	// Serialize input asset.
	Ar << InputAssetComponent;

	// Serialize curve and curve parameters (if we have those).
	Ar << InputCurve;
	Ar << InputCurveParameters;

	// Serialize landscape used for input.
	if(HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_LANDSCAPE_INPUT)
	{
		Ar << InputLandscapeProxy;
	}

	// Create necessary widget resources.
	if(Ar.IsLoading())
	{
		bLoadedParameter = true;

		if(Ar.IsTransacting())
		{
			bInputAssetConnectedInHoudini = bLocalInputAssetConnectedInHoudini;

			if(LocalInputAssetComponent != InputAssetComponent)
			{
				if(InputAssetComponent)
				{
					bInputAssetConnectedInHoudini = false;
				}

				if(LocalInputAssetComponent)
				{
					LocalInputAssetComponent->RemoveDownstreamAsset(HoudiniAssetComponent, InputIndex);
				}
			}
		}
		else
		{
			// If we're loading for real for the first time we need to reset this
			// flag so we can reconnect when we get our parameters uploaded.
			bInputAssetConnectedInHoudini = false;
		}
	}
}


void
UHoudiniAssetInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInput* HoudiniAssetInput = Cast<UHoudiniAssetInput>(InThis);
	if(HoudiniAssetInput)
	{
		// Add reference to held geometry object.
		if(HoudiniAssetInput->InputObject)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputObject, InThis);
		}

		// Add reference to held input asset component, if we have one.
		if(HoudiniAssetInput->InputAssetComponent)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputAssetComponent, InThis);
		}

		// Add reference to held curve object.
		if(HoudiniAssetInput->InputCurve)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputCurve, InThis);
		}

		// Add reference to held landscape.
		if(HoudiniAssetInput->InputLandscapeProxy)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputLandscapeProxy, InThis);
		}

		// Add references for all curve input parameters.
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetInput->InputCurveParameters);
			IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			if(HoudiniAssetParameter)
			{
				Collector.AddReferencedObject(HoudiniAssetParameter, InThis);
			}
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetInput::ClearInputCurveParameters()
{
	for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(InputCurveParameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		if(HoudiniAssetParameter)
		{
			HoudiniAssetParameter->ConditionalBeginDestroy();
		}
	}

	InputCurveParameters.Empty();
}


void
UHoudiniAssetInput::DestroyInputCurve()
{
	// If we have spline, delete it.
	if(InputCurve)
	{
		InputCurve->DetachFromParent();
		InputCurve->UnregisterComponent();
		InputCurve->DestroyComponent();

		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->AttachChildren.Remove(InputCurve);
		}

		InputCurve = nullptr;
	}

	ClearInputCurveParameters();
}


#if WITH_EDITOR

void
UHoudiniAssetInput::OnStaticMeshDropped(UObject* Object)
{
	if(Object != InputObject)
	{
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Input Geometry Change"),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();
		InputObject = Object;
		bStaticMeshChanged = true;
		MarkChanged();

		HoudiniAssetComponent->UpdateEditorProperties(false);
	}
}


bool
UHoudiniAssetInput::OnStaticMeshDraggedOver(const UObject* InObject) const
{
	// We only allow static meshes as geo inputs at this time.
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
	if(InputObject && InputObject->IsA(UStaticMesh::StaticClass()) && GEditor)
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

	TArray<UFactory*> NewAssetFactories;

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(InputObject),
		true,
		AllowedClasses,
		NewAssetFactories,
		OnShouldFilterStaticMesh,
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


TSharedRef<SWidget>
UHoudiniAssetInput::CreateChoiceEntryWidget(TSharedPtr<FString> ChoiceEntry)
{
	FText ChoiceEntryText = FText::FromString(*ChoiceEntry);

	return SNew(STextBlock)
		   .Text(ChoiceEntryText)
		   .ToolTipText(ChoiceEntryText)
		   .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
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


void
UHoudiniAssetInput::CloseStaticMeshComboButton()
{

}


FReply
UHoudiniAssetInput::OnResetStaticMeshClicked()
{
	OnStaticMeshDropped(nullptr);
	return FReply::Handled();
}


void
UHoudiniAssetInput::OnChoiceChange(TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
{
	if(!NewChoice.IsValid())
	{
		return;
	}

	ChoiceStringValue = *(NewChoice.Get());

	// We need to match selection based on label.
	bool bChanged = false;
	int32 ActiveLabel = 0;

	for(int32 LabelIdx = 0; LabelIdx < StringChoiceLabels.Num(); ++LabelIdx)
	{
		FString* ChoiceLabel = StringChoiceLabels[LabelIdx].Get();

		if(ChoiceLabel && ChoiceLabel->Equals(ChoiceStringValue))
		{
			bChanged = true;
			ActiveLabel = LabelIdx;
			break;
		}
	}

	if(bChanged)
	{
		switch(ChoiceIndex)
		{
			case EHoudiniAssetInputType::GeometryInput:
			{
				// We are switching away from geometry input.

				// Reset assigned object.
				InputObject = nullptr;
				break;
			}

			case EHoudiniAssetInputType::AssetInput:
			{
				// We are switching away from asset input.
				DisconnectInputAssetActor();
				break;
			}

			case EHoudiniAssetInputType::CurveInput:
			{
				// We are switching away from curve input.
				DestroyInputCurve();
				break;
			}

			case EHoudiniAssetInputType::LandscapeInput:
			{
				// We are switching away from landscape input.

				// Reset selected landscape.
				InputLandscapeProxy = nullptr;
				break;
			}

			default:
			{
				// Unhandled new input type?
				check(0);
				break;
			}
		}

		// Disconnect currently connected asset.
		DisconnectAndDestroyInputAsset();

		// Switch mode.
		ChoiceIndex = static_cast<EHoudiniAssetInputType::Enum>(ActiveLabel);

		switch(ChoiceIndex)
		{
			case EHoudiniAssetInputType::GeometryInput:
			{
				// We are switching to geometry input.
				break;
			}

			case EHoudiniAssetInputType::AssetInput:
			{
				// We are switching to asset input.
				ConnectInputAssetActor();
				break;
			}

			case EHoudiniAssetInputType::CurveInput:
			{
				// We are switching to curve input.

				// Create new spline component.
				UHoudiniSplineComponent* HoudiniSplineComponent =
					NewObject<UHoudiniSplineComponent>(this, UHoudiniSplineComponent::StaticClass(),
						NAME_None, RF_Public | RF_Transactional);

				HoudiniSplineComponent->AttachTo(HoudiniAssetComponent, NAME_None, EAttachLocation::KeepRelativeOffset);
				HoudiniSplineComponent->RegisterComponent();
				HoudiniSplineComponent->SetVisibility(true);
				HoudiniSplineComponent->SetHoudiniAssetInput(this);

				// Store this component as input curve.
				InputCurve = HoudiniSplineComponent;

				bSwitchedToCurve = true;
				break;
			}

			case EHoudiniAssetInputType::LandscapeInput:
			{
				// We are switching to Landscape input.
				break;
			}

			default:
			{
				// Unhandled new input type?
				check(0);
				break;
			}
		}

		// If we have input object and geometry asset, we need to connect it back.
		MarkPreChanged();
		MarkChanged();
	}
}

bool
UHoudiniAssetInput::OnInputActorFilter(const AActor* const Actor) const
{
	return (Actor && Actor->IsA(AHoudiniAssetActor::StaticClass()));
}


void
UHoudiniAssetInput::OnInputActorSelected(AActor* Actor)
{
	if(!Actor && InputAssetComponent)
	{
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Input Asset Change"),
			HoudiniAssetComponent);
		Modify();

		// Tell the old input asset we are no longer connected.
		InputAssetComponent->RemoveDownstreamAsset(HoudiniAssetComponent, InputIndex);

		// We cleared the selection so just reset all the values.
		InputAssetComponent = nullptr;
		ConnectedAssetId = -1;
	}
	else
	{
		AHoudiniAssetActor* HoudiniAssetActor = (AHoudiniAssetActor*) Actor;
		UHoudiniAssetComponent* ConnectedHoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();

		// If we just selected the already selected Actor do nothing.
		if(ConnectedHoudiniAssetComponent == InputAssetComponent)
		{
			return;
		}

		// Do not allow the input asset to be ourself!
		if(ConnectedHoudiniAssetComponent == HoudiniAssetComponent)
		{
			return;
		}

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Input Asset Change"),
			HoudiniAssetComponent);
		Modify();

		// Tell the old input asset we are no longer connected.
		if(InputAssetComponent)
		{
			InputAssetComponent->RemoveDownstreamAsset(HoudiniAssetComponent, InputIndex);
		}

		InputAssetComponent = ConnectedHoudiniAssetComponent;
		ConnectedAssetId = InputAssetComponent->GetAssetId();

		// Mark as disconnected since we need to reconnect to the new asset.
		bInputAssetConnectedInHoudini = false;
	}

	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetInput::OnInputActorCloseComboButton()
{

}


void
UHoudiniAssetInput::OnInputActorUse()
{

}


bool
UHoudiniAssetInput::OnLandscapeActorFilter(const AActor* const Actor) const
{
	return (Actor && Actor->IsA(ALandscapeProxy::StaticClass()));
}


void
UHoudiniAssetInput::OnLandscapeActorSelected(AActor* Actor)
{
	ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
	if(LandscapeProxy)
	{
		// If we just selected the already selected landscape, do nothing.
		if(LandscapeProxy == InputLandscapeProxy)
		{
			return;
		}

		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Input Landscape Change."),
			HoudiniAssetComponent);
		Modify();

		// Store new landscape.
		InputLandscapeProxy = LandscapeProxy;
	}
	else
	{
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Input Landscape Change."),
			HoudiniAssetComponent);
		Modify();

		InputLandscapeProxy = nullptr;
	}

	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetInput::OnLandscapeActorCloseComboButton()
{

}


void
UHoudiniAssetInput::OnLandscapeActorUse()
{

}

#endif

void
UHoudiniAssetInput::ConnectInputAssetActor()
{
	if(InputAssetComponent && FHoudiniEngineUtils::IsValidAssetId(InputAssetComponent->GetAssetId())
		&& !bInputAssetConnectedInHoudini)
	{
		FHoudiniEngineUtils::HapiConnectAsset(
			InputAssetComponent->GetAssetId(),
			0, // We just pick the first OBJ since we have no way letting the user pick.
			HoudiniAssetComponent->GetAssetId(),
			InputIndex
		);

		ConnectedAssetId = InputAssetComponent->GetAssetId();

		InputAssetComponent->AddDownstreamAsset(HoudiniAssetComponent, InputIndex);
		bInputAssetConnectedInHoudini = true;
	}
}


void
UHoudiniAssetInput::DisconnectInputAssetActor()
{
	if(bInputAssetConnectedInHoudini && !InputAssetComponent)
	{
		FHoudiniEngineUtils::HapiDisconnectAsset(HoudiniAssetComponent->GetAssetId(), InputIndex);
		bInputAssetConnectedInHoudini = false;
	}
}


void
UHoudiniAssetInput::ConnectLandscapeActor()
{

}


void
UHoudiniAssetInput::DisconnectLandscapeActor()
{

}


HAPI_AssetId
UHoudiniAssetInput::GetConnectedAssetId() const
{
	return ConnectedAssetId;
}


bool
UHoudiniAssetInput::IsGeometryAssetConnected() const
{
	if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId))
	{
		if(InputObject && InputObject->IsA(UStaticMesh::StaticClass()))
		{
			return true;
		}
	}

	return false;
}


bool
UHoudiniAssetInput::IsInputAssetConnected() const
{
	if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId) && InputAssetComponent && bInputAssetConnectedInHoudini)
	{
		return true;
	}

	return false;
}


bool
UHoudiniAssetInput::IsCurveAssetConnected() const
{
	if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId))
	{
		if(InputCurve)
		{
			return true;
		}
	}

	return false;
}


bool
UHoudiniAssetInput::IsLandscapeAssetConnected() const
{
	if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId))
	{
		if(EHoudiniAssetInputType::LandscapeInput == ChoiceIndex)
		{
			return true;
		}
	}

	return false;
}


void
UHoudiniAssetInput::OnInputCurveChanged()
{
	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetInput::ExternalDisconnectInputAssetActor()
{
	InputAssetComponent = nullptr;
	ConnectedAssetId = -1;

	MarkPreChanged();
	MarkChanged();
}


bool
UHoudiniAssetInput::DoesInputAssetNeedInstantiation()
{
	if(ChoiceIndex != EHoudiniAssetInputType::AssetInput)
	{
		return false;
	}

	if(InputAssetComponent == nullptr)
	{
		return false;
	}

	if(!FHoudiniEngineUtils::IsValidAssetId(InputAssetComponent->GetAssetId()))
	{
		return true;
	}

	return false;
}


UHoudiniAssetComponent*
UHoudiniAssetInput::GetConnectedInputAssetComponent()
{
	return InputAssetComponent;
}


void
UHoudiniAssetInput::NotifyChildParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	if(HoudiniAssetParameter && EHoudiniAssetInputType::CurveInput == ChoiceIndex)
	{
		MarkPreChanged();
		
		if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId))
		{
			// We need to upload changed param back to HAPI.
			HoudiniAssetParameter->UploadParameterValue();
		}

		MarkChanged();
	}
}


void
UHoudiniAssetInput::UpdateInputCurve()
{
	FString CurvePointsString;
	EHoudiniSplineComponentType::Enum CurveTypeValue = EHoudiniSplineComponentType::Bezier;
	EHoudiniSplineComponentMethod::Enum CurveMethodValue = EHoudiniSplineComponentMethod::CVs;
	int32 CurveClosed = 1;

	HAPI_NodeId NodeId = -1;
	if(FHoudiniEngineUtils::HapiGetNodeId(ConnectedAssetId, 0, 0, NodeId))
	{
		FHoudiniEngineUtils::HapiGetParameterDataAsString(NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT(""),
			CurvePointsString);
		FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_TYPE,
			(int32) EHoudiniSplineComponentType::Bezier, (int32&) CurveTypeValue);
		FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_METHOD,
			(int32) EHoudiniSplineComponentMethod::CVs, (int32&) CurveMethodValue);
		FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed);
	}

	// Construct geo part object.
	FHoudiniGeoPartObject HoudiniGeoPartObject(ConnectedAssetId, 0, 0, 0);
	HoudiniGeoPartObject.bIsCurve = true;

	HAPI_AttributeInfo AttributeRefinedCurvePositions;
	TArray<float> RefinedCurvePositions;
	FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION,
		AttributeRefinedCurvePositions, RefinedCurvePositions);

	// Process coords string and extract positions.
	TArray<FVector> CurvePoints;
	FHoudiniEngineUtils::ExtractStringPositions(CurvePointsString, CurvePoints);

	TArray<FVector> CurveDisplayPoints;
	FHoudiniEngineUtils::ConvertScaleAndFlipVectorData(RefinedCurvePositions, CurveDisplayPoints);

	InputCurve->Construct(HoudiniGeoPartObject, CurvePoints, CurveDisplayPoints, CurveTypeValue, CurveMethodValue,
		(CurveClosed == 1));

	// We also need to construct curve parameters we care about.
	TMap<FString, UHoudiniAssetParameter*> NewInputCurveParameters;

	{
		HAPI_NodeInfo NodeInfo;
		HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId,
			&NodeInfo), false);

		TArray<HAPI_ParmInfo> ParmInfos;
		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeId,
			&ParmInfos[0], 0, NodeInfo.parmCount), false);

		// Retrieve integer values for this asset.
		TArray<int32> ParmValueInts;
		ParmValueInts.SetNumZeroed(NodeInfo.parmIntValueCount);
		if(NodeInfo.parmIntValueCount > 0)
		{
			HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParmIntValues(FHoudiniEngine::Get().GetSession(),
				NodeId, &ParmValueInts[0], 0, NodeInfo.parmIntValueCount), false);
		}

		// Retrieve float values for this asset.
		TArray<float> ParmValueFloats;
		ParmValueFloats.SetNumZeroed(NodeInfo.parmFloatValueCount);
		if(NodeInfo.parmFloatValueCount > 0)
		{
			HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(),
				NodeId, &ParmValueFloats[0], 0, NodeInfo.parmFloatValueCount), false);
		}

		// Retrieve string values for this asset.
		TArray<HAPI_StringHandle> ParmValueStrings;
		ParmValueStrings.SetNumZeroed(NodeInfo.parmStringValueCount);
		if(NodeInfo.parmStringValueCount > 0)
		{
			HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(),
				NodeId, true, &ParmValueStrings[0], 0, NodeInfo.parmStringValueCount), false);
		}

		// Create properties for parameters.
		for(int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
		{
			// Retrieve param info at this index.
			const HAPI_ParmInfo& ParmInfo = ParmInfos[ParamIdx];

			// If parameter is invisible, skip it.
			if(ParmInfo.invisible)
			{
				continue;
			}

			FString ParameterName;
			FHoudiniEngineString HoudiniEngineString(ParmInfo.nameSH);
			if(!HoudiniEngineString.ToFString(ParameterName))
			{
				// We had trouble retrieving name of this parameter, skip it.
				continue;
			}

			// See if it's one of parameters we are interested in.
			if(!ParameterName.Equals(TEXT(HAPI_UNREAL_PARAM_CURVE_METHOD)) &&
				!ParameterName.Equals(TEXT(HAPI_UNREAL_PARAM_CURVE_TYPE)) &&
				!ParameterName.Equals(TEXT(HAPI_UNREAL_PARAM_CURVE_CLOSED)))
			{
				// Not parameter we are interested in.
				continue;
			}

			// See if this parameter has already been created.
			UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = InputCurveParameters.Find(ParameterName);
			UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

			// If parameter exists, we can reuse it.
			if(FoundHoudiniAssetParameter)
			{
				HoudiniAssetParameter = *FoundHoudiniAssetParameter;

				// Remove parameter from current map.
				InputCurveParameters.Remove(ParameterName);

				// Reinitialize parameter and add it to map.
				HoudiniAssetParameter->CreateParameter(nullptr, this, NodeId, ParmInfo);
				NewInputCurveParameters.Add(ParameterName, HoudiniAssetParameter);
				continue;
			}
			else
			{
				if(HAPI_PARMTYPE_INT == ParmInfo.type)
				{
					if(!ParmInfo.choiceCount)
					{
						HoudiniAssetParameter = UHoudiniAssetParameterInt::Create(nullptr, this, NodeId, ParmInfo);
					}
					else
					{
						HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(nullptr, this, NodeId, ParmInfo);
					}
				}
				else if(HAPI_PARMTYPE_TOGGLE == ParmInfo.type)
				{
					HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create(nullptr, this, NodeId, ParmInfo);
				}
				else
				{
					check(false);
				}

				NewInputCurveParameters.Add(ParameterName, HoudiniAssetParameter);
			}
		}

		ClearInputCurveParameters();
		InputCurveParameters = NewInputCurveParameters;
	}

	if(bSwitchedToCurve)
	{

#if WITH_EDITOR

		// We need to trigger details panel update.
		HoudiniAssetComponent->UpdateEditorProperties(false);
#endif

		bSwitchedToCurve = false;
	}
}


FText
UHoudiniAssetInput::HandleChoiceContentText() const
{
	return FText::FromString(ChoiceStringValue);
}


#if WITH_EDITOR

void
UHoudiniAssetInput::CheckStateChangedExportOnlySelected(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeInputSelectionOnly != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Selection mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeInputSelectionOnly = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportOnlySelected() const
{
	if(bLandscapeInputSelectionOnly)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedExportCurves(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeExportCurves != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Curve mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeExportCurves = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportCurves() const
{
	if(bLandscapeExportCurves)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedExportFullGeometry(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeExportFullGeometry != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Full Geometry mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeExportFullGeometry = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportFullGeometry() const
{
	if(bLandscapeExportFullGeometry)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedExportMaterials(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeExportMaterials != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Materials mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeExportMaterials = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportMaterials() const
{
	if(bLandscapeExportMaterials)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedExportLighting(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeExportLighting != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Lighting mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeExportLighting = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportLighting() const
{
	if(bLandscapeExportLighting)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedExportNormalizedUVs(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeExportNormalizedUVs != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Normalized UVs mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeExportNormalizedUVs = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportNormalizedUVs() const
{
	if(bLandscapeExportNormalizedUVs)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedExportTileUVs(ECheckBoxState NewState)
{
	int32 bState = (ECheckBoxState::Checked == NewState);

	if(bLandscapeExportTileUVs != bState)
	{
		// Record undo information.
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInputChange", "Houdini Export Landscape Tile UVs mode change."),
			HoudiniAssetComponent);
		Modify();

		MarkPreChanged();

		bLandscapeExportTileUVs = bState;

		// Mark this parameter as changed.
		MarkChanged();
	}
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportTileUVs() const
{
	if(bLandscapeExportTileUVs)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


FReply
UHoudiniAssetInput::OnButtonClickRecommit()
{
	// There's no undo operation for button.

	MarkPreChanged();
	MarkChanged();

	return FReply::Handled();
}

#endif
