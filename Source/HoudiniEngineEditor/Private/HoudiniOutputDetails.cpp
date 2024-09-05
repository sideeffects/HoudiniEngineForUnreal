
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

#include "HoudiniOutputDetails.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineOutputStats.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniStaticMesh.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SplineComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailGroup.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Particles/ParticleSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "SAssetDropTarget.h"
#include "ScopedTransaction.h"
#include "Sound/SoundBase.h"
#include "UnrealEdGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
	#include "Engine/SkinnedAssetCommon.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	#include "GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollection/GeometryCollectionObject.h"
#else
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
	#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"	
#endif

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniOutputDetails::CreateWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	const TArray<TWeakObjectPtr<UHoudiniOutput>>& InOutputs)
{
	if (InOutputs.Num() <= 0)
		return;

	const TWeakObjectPtr<UHoudiniOutput>& MainOutput = InOutputs[0];
	if (!IsValidWeakPointer(MainOutput))
		return;

	// Don't create UI for editable curve.
	if (MainOutput->IsEditableNode() && MainOutput->GetType() == EHoudiniOutputType::Curve)
		return;

	// Get thumbnail pool for this builder.
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = HouOutputCategory.GetParentLayout().GetThumbnailPool();

	switch (MainOutput->GetType()) 
	{
		case EHoudiniOutputType::Mesh: 
		{
			FHoudiniOutputDetails::CreateMeshOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::Landscape: 
		{
			FHoudiniOutputDetails::CreateLandscapeOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::Instancer:
		{
			FHoudiniOutputDetails::CreateInstancerOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::Curve:
		{
			FHoudiniOutputDetails::CreateCurveOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::GeometryCollection:
		{
			FHoudiniOutputDetails::CreateGeometryCollectionOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
		case EHoudiniOutputType::DataTable:
		{
			FHoudiniOutputDetails::CreateDataTableOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
		case EHoudiniOutputType::LandscapeSpline:
		{
			FHoudiniOutputDetails::CreateLandscapeSplineOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
		case EHoudiniOutputType::AnimSequence:
		{
			FHoudiniOutputDetails::CreateAnimSequenceOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
		case EHoudiniOutputType::Skeletal:
		default: 
		{
			FHoudiniOutputDetails::CreateSkeletalOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
	}
}


void 
FHoudiniOutputDetails::CreateLandscapeOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	// Go through this output's objects
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& CurrentOutputObj : OutputObjects) 
	{
		FHoudiniOutputObjectIdentifier& Identifier = CurrentOutputObj.Key;
		const FHoudiniGeoPartObject *HGPO = nullptr;
		for (const auto& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
		{
			if (!Identifier.Matches(CurHGPO))
				continue;

			HGPO = &CurHGPO;
			break;
		}
		
		if (!HGPO)
			continue;
		
		
		if (UHoudiniLandscapePtr* LandscapePointer = Cast<UHoudiniLandscapePtr>(CurrentOutputObj.Value.OutputObject))
		{
			CreateLandscapeOutputWidget_Helper(HouOutputCategory, InOutput, *HGPO, LandscapePointer, Identifier);
		}
		else if (UHoudiniLandscapeTargetLayerOutput* LayerOutput = Cast<UHoudiniLandscapeTargetLayerOutput>(CurrentOutputObj.Value.OutputObject))
		{
			CreateLandscapeEditLayerOutputWidget_Helper(HouOutputCategory, InOutput, *HGPO, LayerOutput, Identifier);
		}
	}
}

void
FHoudiniOutputDetails::CreateLandscapeOutputWidget_Helper(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput,
	const FHoudiniGeoPartObject& HGPO,
	const TWeakObjectPtr<UHoudiniLandscapePtr>& LandscapePointer,
	const FHoudiniOutputObjectIdentifier& OutputIdentifier)
{
	if (!LandscapePointer.IsValid() || !LandscapePointer->LandscapeSoftPtr.IsValid())
		return;

	if (!IsValidWeakPointer(InOutput))
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!IsValidWeakPointer(HAC))
		return;

	const TWeakObjectPtr<AActor>& OwnerActor = HAC->GetOwner();
	if (!IsValidWeakPointer(OwnerActor))
		return;

	const TWeakObjectPtr<ALandscapeProxy>& Landscape = LandscapePointer->LandscapeSoftPtr.Get();
	if (!IsValidWeakPointer(Landscape))
		return;

	// TODO: Get bake base name
	FString Label = Landscape->GetName();

	if (!LandscapePointer->EditLayerName.IsNone())
	{
		Label = FString::Format(TEXT("{0} ({1})"), {Label, (LandscapePointer->EditLayerName.ToString())});
	}

	EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType = LandscapePointer->BakeType;

	// Create a group for this output object
	IDetailGroup& LandscapeGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));
	
	// --------------------------------
	// Modified edit layers, if there are any.
	// --------------------------------

	if (!LandscapePointer->EditLayerName.IsNone())
	{
		// Create label to display the edit layer name.

		LandscapeGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeOutputEditLayers", "Edit Layer"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(STextBlock)
				.Text(FText::AsCultureInvariant(FText::AsCultureInvariant(LandscapePointer->EditLayerName.ToString())) )
				.Font(IDetailLayoutBuilder::GetDetailFont())
		];
	}

	

	// --------------------------------
	// Bake options for landscape tile
	// --------------------------------

	// Get thumbnail pool for this builder
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	TArray<TSharedPtr<FString>>* BakeOptionString = FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels();

	// Create bake mesh name textfield.
	
	LandscapeGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(Label))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.OnTextCommitted_Lambda([InOutput, OutputIdentifier](const FText& Val, ETextCommit::Type TextCommitType)
			{
				if (InOutput.IsValid())
				{
					FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
					FHoudiniEngineUtils::UpdateEditorProperties(true);
				}
			})
		]
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]() 
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];

	// Create the thumbnail for the landscape output object.
	TSharedPtr< FAssetThumbnail > LandscapeThumbnail =
		MakeShareable(new FAssetThumbnail(Landscape.Get(), 64, 64, AssetThumbnailPool));

	TSharedPtr< SBorder > LandscapeThumbnailBorder;
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	LandscapeGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(SSpacer)
		.Size(FVector2D(250, 64))
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];

	VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
	[
		SNew(SBox).WidthOverride(175)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 2.0f, 0.0f)
			.AutoWidth()
			[
				SAssignNew(LandscapeThumbnailBorder, SBorder)
				.Padding(5.0f)
				.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) Landscape)
				.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) Landscape)
				[
					SNew(SBox)
					.WidthOverride(64)
					.HeightOverride(64)
					.ToolTipText(FText::FromString(Landscape->GetPathName()))
					[
						LandscapeThumbnail->MakeThumbnailWidget()
					]
				]
			]

			+ SHorizontalBox::Slot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				//.WidthOverride(40.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("BakeOutputLandscape", "Bake Output"))
					.IsEnabled(true)
					.OnClicked_Lambda([InOutput, OutputIdentifier, HAC, HGPO, Landscape, LandscapeOutputBakeType]()
					{
						if (!InOutput.IsValid() || !HAC.IsValid() || !Landscape.IsValid())
							return FReply::Handled();

						FHoudiniBakeSettings BakeSettings;
						BakeSettings.SetFromHAC(HAC.Get());

						FHoudiniOutputObject const* const FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
						if (FoundOutputObject)
						{
							TArray<UHoudiniOutput*> AllOutputs;
							AllOutputs.Reserve(HAC->GetNumOutputs());
							HAC->GetOutputs(AllOutputs);
							FHoudiniOutputDetails::OnBakeOutputObject(
								FoundOutputObject->BakeName,
								Landscape.Get(),
								OutputIdentifier,
								*FoundOutputObject,
								HGPO,
								HAC.Get(),
								InOutput.Get(),
								HAC->BakeFolder.Path,
								BakeSettings,
								HAC->TemporaryCookFolder.Path,
								LandscapeOutputBakeType,
								AllOutputs);
						}

						// TODO: Remove the output landscape if the landscape bake type is Detachment?
						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("HoudiniLandscapeBakeButton", "Bake this output Landscape to a new Landscape Actor."))	
				]	
			]
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(120.f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels())
					.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels())[(uint8)LandscapeOutputBakeType])
					.OnGenerateWidget_Lambda(
						[](TSharedPtr< FString > InItem)
					{
						return SNew(STextBlock).Text(FText::FromString(*InItem));
					})
					.OnSelectionChanged_Lambda(
						[LandscapePointer, InOutput](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
						{
							if (SelectType != ESelectInfo::Type::OnMouseClick)
								return;

							FString *NewChoiceStr = NewChoice.Get();
							if (!NewChoiceStr)
								return;

							if (LandscapePointer.IsValid())
							{
								if (*NewChoiceStr == FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(EHoudiniLandscapeOutputBakeType::Detachment))
								{
									LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::Detachment);
								}
								else if (*NewChoiceStr == FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(EHoudiniLandscapeOutputBakeType::BakeToImage))
								{
									LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::BakeToImage);
								}
								else
								{
									LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::BakeToWorld);
								}
							}

							if (InOutput.IsValid())
								FHoudiniEngineUtils::UpdateEditorProperties(true);
						})
					[
						SNew(STextBlock)
						.Text_Lambda([LandscapePointer]()
						{
							if (!IsValidWeakPointer(LandscapePointer))
								return FText();
							
							const FString BakeTypeString = FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(LandscapePointer->GetLandscapeOutputBakeType());
							return FText::FromString(BakeTypeString);
						})
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]		
			]
		]
	];

	// Store thumbnail for this landscape.
	OutputObjectThumbnailBorders.Add(Landscape, LandscapeThumbnailBorder);

	// We need to add material box for each the landscape and landscape hole materials
	for (int32 MaterialIdx = 0; MaterialIdx < 2; ++MaterialIdx)
	{
		UMaterialInterface * MaterialInterface = MaterialIdx == 0 ? Landscape->GetLandscapeMaterial() : Landscape->GetLandscapeHoleMaterial();
		TSharedPtr<SBorder> MaterialThumbnailBorder;
		TSharedPtr<SHorizontalBox> HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if (MaterialInterface)
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}

		// Create thumbnail for this material.
		TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
			MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(STextBlock)
			.Text(MaterialIdx == 0 ? LOCTEXT("LandscapeMaterial", "Landscape Material") : LOCTEXT("LandscapeHoleMaterial", "Landscape Hole Material"))
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

		VerticalBox->AddSlot().Padding(0, 2)
		[
			SNew(SAssetDropTarget)
			.OnAreAssetsAcceptableForDrop(this, &FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver)
			.OnAssetsDropped(this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, Landscape, InOutput, MaterialIdx)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
		[
			SAssignNew(MaterialThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (const TWeakObjectPtr<UObject>&) Landscape, MaterialIdx)
			.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) MaterialInterface)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(MaterialPathName))
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this landscape and material index.
		{
			TPairInitializer<TWeakObjectPtr<ALandscapeProxy>, int32> Pair(Landscape, MaterialIdx);
			MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
		}

		// Combox Box and Button Box
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Combo row
		TSharedPtr< SComboButton > AssetComboButton;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(AssetComboButton, SComboButton)
				//.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
				.ButtonStyle(_GetEditorStyle(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(_GetEditorStyle().GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent, TWeakObjectPtr<UMaterialInterface>(MaterialInterface), (TWeakObjectPtr<UObject>)Landscape, InOutput, MaterialIdx)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
					.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];

		// Buttons row
		TSharedPtr<SHorizontalBox> ButtonBox;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];

		// Add use Content Browser selection arrow
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(const TWeakObjectPtr<UObject>&)Landscape, InOutput, MaterialIdx),
				TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")))
		];

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)MaterialInterface),
				TAttribute< FText >(MaterialTooltip))
		];

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("ResetToBaseMaterial", "Reset to base material"))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked(	this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, Landscape, InOutput, MaterialIdx)
			[
				SNew(SImage)
				.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];

		// Store combo button for this mesh and index.
		{
			TPairInitializer<TWeakObjectPtr<ALandscapeProxy>, int32> Pair(Landscape, MaterialIdx);
			MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
		}
	}
}

void FHoudiniOutputDetails::CreateLandscapeEditLayerOutputWidget_Helper(IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput, const FHoudiniGeoPartObject& HGPO,
	const TWeakObjectPtr<UHoudiniLandscapeTargetLayerOutput>& LandscapeEditLayer, const FHoudiniOutputObjectIdentifier& OutputIdentifier)
{
	const UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());

	// Check everything is valid first.
	if (!LandscapeEditLayer.IsValid() || !IsValid(LandscapeEditLayer->Landscape) || !IsValidWeakPointer(InOutput) ||
		!IsValid(HAC))
		return;

	ALandscape* Landscape = LandscapeEditLayer->Landscape;

	const FString Label = Landscape->GetName();
	const FString EditLayerName = Landscape->bHasLayersContent ? LandscapeEditLayer->BakedEditLayer :
		"Target landscape does not support edit layers.";

	const FString TargetLayerName = LandscapeEditLayer->TargetLayer;

	// Get thumbnail pool for this builder
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
	IDetailGroup& LandscapeGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));

	//------------------------------------------------------------------------------------------------------------------------------
	// Landscape information
	//------------------------------------------------------------------------------------------------------------------------------

	FString TargetLandscape = Landscape->GetActorLabel();
	FString OutputMode = LandscapeEditLayer->bCreatedLandscape ? "Create Landscape" : "Modify Landscape";

	LandscapeGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeEditLayerName", "Output Mode"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(OutputMode))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	LandscapeGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeEditLayerName", "Target Landscape"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(TargetLandscape))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	//------------------------------------------------------------------------------------------------------------------------------
	// Create labels to display the edit layer name.
	//------------------------------------------------------------------------------------------------------------------------------

	LandscapeGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeEditLayerName", "Edit Layer Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(EditLayerName))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	LandscapeGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeEditLayerName", "Target Layer Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(TargetLayerName))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	//------------------------------------------------------------------------------------------------------------------------------
	// Size Information
	//------------------------------------------------------------------------------------------------------------------------------

	const FHoudiniExtents & Extents = LandscapeEditLayer->Extents;

	FString SizeInfo = FString::Printf(TEXT("(%d,%d) -> (%d,%d)"), Extents.Min.X, Extents.Min.Y, Extents.Max.X, Extents.Max.Y);

	LandscapeGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeEditLayerName", "Modified Extents"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(SizeInfo))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

void
FHoudiniOutputDetails::CreateMeshOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!IsValidWeakPointer(HAC))
		return;

	// Go through this output's object
	int32 OutputObjIdx = 0;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& IterObject : OutputObjects)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(IterObject.Value.OutputObject);
		UHoudiniStaticMesh* ProxyMesh = Cast<UHoudiniStaticMesh>(IterObject.Value.ProxyObject);

		if ((!IsValid(StaticMesh))
			&& (!IsValid(ProxyMesh)))
			continue;

		// Do not display geometry collection static meshes.
		if (IterObject.Value.bIsGeometryCollectionPiece)
			continue;

		FHoudiniOutputObjectIdentifier & OutputIdentifier = IterObject.Key;

		// Find the corresponding HGPO in the output
		FHoudiniGeoPartObject HoudiniGeoPartObject;
		for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects())
		{
			if (!OutputIdentifier.Matches(curHGPO))
				continue;

			HoudiniGeoPartObject = curHGPO;
			break;
		}

		bool bIsProxyMeshCurrent = IterObject.Value.bProxyIsCurrent;
		if (IsValid(StaticMesh) && !bIsProxyMeshCurrent)
		{
			// If we have a static mesh, display its widget unless the proxy is more recent
			CreateStaticMeshAndMaterialWidgets(
				HouOutputCategory, InOutput, StaticMesh, OutputIdentifier, HoudiniGeoPartObject, bIsProxyMeshCurrent);
		}
		else
		{
			// If we only have a proxy mesh, then show the proxy widget
			CreateProxyMeshAndMaterialWidgets(
				HouOutputCategory, InOutput, ProxyMesh, OutputIdentifier, HoudiniGeoPartObject);
		}
	}
}


void
FHoudiniOutputDetails::CreateSkeletalOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!IsValidWeakPointer(HAC))
		return;

	// See if we have a Skeletal Mesh and/or skeleton.
	USkeletalMesh* SkelMesh = nullptr;
	USkeleton* Skeleton = nullptr;
	UPhysicsAsset * PhysicsAsset = nullptr;
	FHoudiniOutputObjectIdentifier OutputIdentifier;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& IterObject : OutputObjects)
	{
		if (!SkelMesh)
		{
			SkelMesh = Cast<USkeletalMesh>(IterObject.Value.OutputObject);
			OutputIdentifier = IterObject.Key;
		}
		if (!Skeleton)
		{
			Skeleton = Cast<USkeleton>(IterObject.Value.OutputObject);

			if (SkelMesh == nullptr)
				OutputIdentifier = IterObject.Key;
		}
		if (!PhysicsAsset)
		{
			PhysicsAsset = Cast<UPhysicsAsset>(IterObject.Value.OutputObject);

			if (SkelMesh == nullptr)
				OutputIdentifier = IterObject.Key;
		}
	}

	if ((!IsValid(SkelMesh)) && (!IsValid(Skeleton)))
			return;

	// Find the corresponding HGPO in the output
	FHoudiniGeoPartObject HoudiniGeoPartObject;
	for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		if (OutputIdentifier.Matches(curHGPO))
		{
			HoudiniGeoPartObject = curHGPO;
			break;
		}
	}

	// If we have a Skeletal mesh, display its widget unless the proxy is more recent
	CreateSkeletalMeshAndMaterialWidgets(HouOutputCategory, InOutput, SkelMesh, Skeleton, PhysicsAsset, OutputIdentifier, HoudiniGeoPartObject);
}

void FHoudiniOutputDetails::CreateAnimSequenceOutputWidget(IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	if (!IsValidWeakPointer(InOutput))
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	int32 Idx = 1;
	for (auto& IterObject : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = IterObject.Value;
		
	    if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(CurrentOutputObject.OutputObject))
	    {
	    	if (!IsValid(AnimSequence))
	    	{
	    		continue;
	    	}
	    	FString Label = AnimSequence->GetName();
	    	IDetailGroup& OutputGrp = HouOutputCategory.AddGroup(FName(Label), FText::FromString(Label), false, false);
		    TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
		    TSharedPtr<SBorder> ThumbnailBorder;
		    IDetailLayoutBuilder& DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
		    TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
		    TSharedPtr< FAssetThumbnail > AnimThumbnail =
			    MakeShareable(new FAssetThumbnail(AnimSequence, 64, 64, AssetThumbnailPool));

		    OutputGrp.AddWidgetRow()
		    .NameContent()
		    [
			    SNew(STextBlock)
			    .Text(FText::FromString("Anim Sequence"))
			    .Font(IDetailLayoutBuilder::GetDetailFont())
		    ]
		    .ValueContent()
		    .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		    [
			    VerticalBox
		    ];

		    VerticalBox->AddSlot()
		    .Padding(0, 2)
		    .AutoHeight()
		    [
			    SNew(SHorizontalBox)
			    + SHorizontalBox::Slot()
			    .Padding(0.0f, 0.0f, 2.0f, 0.0f)
			    .AutoWidth()
			    [
				    SAssignNew(ThumbnailBorder, SBorder)
					    .Padding(5.0f)
					    .BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) AnimSequence)
					    .OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) AnimSequence)
				    [
					    SNew(SBox)
						    .WidthOverride(64)
						    .HeightOverride(64)
						    .ToolTipText(FText::FromString(AnimSequence->GetPathName()))
					    [
						    AnimThumbnail->MakeThumbnailWidget()
					    ]
				    ]
			    ]
			    + SHorizontalBox::Slot()
			    .AutoWidth()
			    .Padding(2.0f, 0.0f)
			    .VAlign(VAlign_Center)
			    [
				    SNew(SVerticalBox)
				    + SVerticalBox::Slot()
				    .AutoHeight()
				    .Padding(2.0f, 0.0f)
				    .VAlign(VAlign_Center)
				    [
					    SNew(STextBlock)
					    .Text(FText::FromName(AnimSequence->GetFName()))
					    .Font(IDetailLayoutBuilder::GetDetailFont())
				    ]
				    + SVerticalBox::Slot()
				    .AutoHeight()
				    .Padding(2, 0)
				    .VAlign(VAlign_Center)
				    .HAlign(HAlign_Left)
				    [
					    PropertyCustomizationHelpers::MakeBrowseButton(
						    FSimpleDelegate::CreateSP(
							    this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&) AnimSequence),
						    TAttribute<FText>(LOCTEXT("HoudiniAnimSequenceBrowseButton", "Browse to this asset in the content browser")))
				    ]
			    ]
		    ];

		    // OutputGrp.AddWidgetRow()
		    // .NameContent()
		    // [
			   //  SNew(STextBlock)
			   //  .Text(FText::FromString("Row Struct"))
			   //  .Font(IDetailLayoutBuilder::GetDetailFont())
		    // ]
		    // .ValueContent()
		    // .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		    // [
			   //  RSVerticalBox
		    // ];
		    //
		    // RSVerticalBox->AddSlot()
		    // .Padding(0, 2)
		    // .AutoHeight()
		    // [
			   //  SNew(SHorizontalBox)
			   //  + SHorizontalBox::Slot()
			   //  .Padding(0.0f, 0.0f, 2.0f, 0.0f)
			   //  .AutoWidth()
			   //  [
				  //   SAssignNew(RSThumbnailBorder, SBorder)
				  //   .Padding(5.0f)
				  //   .BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) DT->RowStruct)
				  //   .OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) DT->RowStruct)
				  //   [
					 //    SNew(SBox)
					 //    .WidthOverride(64)
					 //    .HeightOverride(64)
					 //    .ToolTipText(FText::FromName(DT->RowStruct->GetFName()))
					 //    [
						//     RSThumbnail->MakeThumbnailWidget()
					 //    ]
				  //   ]
			   //  ]
			   //  + SHorizontalBox::Slot()
			   //  .AutoWidth()
			   //  .Padding(2.0f, 0.0f)
			   //  .VAlign(VAlign_Center)
			   //  [
				  //   SNew(SVerticalBox)
				  //   + SVerticalBox::Slot()
				  //   .AutoHeight()
				  //   .Padding(2.0f, 0.0f)
				  //   .VAlign(VAlign_Center)
				  //   [
					 //    SNew(STextBlock)
					 //    .Text(FText::FromName(DT->RowStruct->GetFName()))
					 //    .Font(IDetailLayoutBuilder::GetDetailFont())
				  //   ]
				  //   + SVerticalBox::Slot()
				  //   .AutoHeight()
				  //   .Padding(2.0f, 0.0f)
				  //   .VAlign(VAlign_Center)
				  //   .HAlign(HAlign_Left)
				  //   [
					 //    PropertyCustomizationHelpers::MakeBrowseButton(
						//     FSimpleDelegate::CreateSP(
						// 	    this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&) DT->RowStruct),
						//     TAttribute<FText>(LOCTEXT("HoudiniDataTableRowStructBrowseButton", "Browse to the generated Data Table's row struct in the content browser")))
				  //   ]
			   //  ]
			   //  
		    // ];

		    OutputObjectThumbnailBorders.Add((UObject*) AnimSequence, ThumbnailBorder);
	    }
	}
}

void 
FHoudiniOutputDetails::CreateCurveOutputWidget(IDetailCategoryBuilder& HouOutputCategory, const TWeakObjectPtr<UHoudiniOutput>& InOutput) 
{
	if (!IsValidWeakPointer(InOutput))
		return;

	int32 OutputObjIdx = 0;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& IterObject : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = IterObject.Value;
		for(auto Component : IterObject.Value.OutputComponents)
		{
		    USceneComponent* SplineComponent = Cast<USceneComponent>(Component);
		    if (!IsValid(SplineComponent))
			    continue;

		    FHoudiniOutputObjectIdentifier& OutputIdentifier = IterObject.Key;
		    FHoudiniGeoPartObject HoudiniGeoPartObject;
		    for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects()) 
		    {
			    if (!OutputIdentifier.Matches(curHGPO))
				    continue;

			    HoudiniGeoPartObject = curHGPO;
			    break;
		    }

		    CreateCurveWidgets(HouOutputCategory, InOutput, SplineComponent, CurrentOutputObject, OutputIdentifier, HoudiniGeoPartObject);
		}
	}
}

void FHoudiniOutputDetails::CreateGeometryCollectionOutputWidget(IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	
	for (auto& IterObject : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = IterObject.Value;

		AGeometryCollectionActor * GeometryCollectionActor = Cast<AGeometryCollectionActor>(IterObject.Value.OutputObject);

		if (!IsValid(GeometryCollectionActor))
			continue;

		FHoudiniOutputObjectIdentifier& OutputIdentifier = IterObject.Key;
		FHoudiniGeoPartObject HoudiniGeoPartObject;
		for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects()) 
		{
			if (!OutputIdentifier.Matches(curHGPO))
				continue;

			HoudiniGeoPartObject = curHGPO;
			break;
		}

		CreateGeometryCollectionWidgets(HouOutputCategory, InOutput, GeometryCollectionActor, CurrentOutputObject, OutputIdentifier, HoudiniGeoPartObject);
	}
}

void
FHoudiniOutputDetails::CreateDataTableOutputWidget(IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	int32 Idx = 1;
	for (auto& IterObject : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = IterObject.Value;

	    if (UDataTable* DT = Cast<UDataTable>(CurrentOutputObject.OutputObject))
	    {
		    FString DTIndex = "Data Table Output " + FString::FromInt(Idx++);
		    IDetailGroup& DTOutputGrp = HouOutputCategory.AddGroup(FName(DTIndex), FText::FromString(DTIndex), false, false);
		    TSharedRef<SVerticalBox> DTVerticalBox = SNew(SVerticalBox);
		    TSharedRef<SVerticalBox> RSVerticalBox = SNew(SVerticalBox);
		    TSharedPtr<SBorder> DTThumbnailBorder;
		    TSharedPtr<SBorder> RSThumbnailBorder;
		    IDetailLayoutBuilder& DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
		    TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
		    TSharedPtr< FAssetThumbnail > DTThumbnail =
			    MakeShareable(new FAssetThumbnail(DT, 64, 64, AssetThumbnailPool));
		    TSharedPtr< FAssetThumbnail > RSThumbnail =
			    MakeShareable(new FAssetThumbnail(DT->RowStruct, 64, 64, AssetThumbnailPool));

		    DTOutputGrp.AddWidgetRow()
		    .NameContent()
		    [
			    SNew(STextBlock)
			    .Text(FText::FromString("Data Table"))
			    .Font(IDetailLayoutBuilder::GetDetailFont())
		    ]
		    .ValueContent()
		    .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		    [
			    DTVerticalBox
		    ];

		    DTVerticalBox->AddSlot()
		    .Padding(0, 2)
		    .AutoHeight()
		    [
			    SNew(SHorizontalBox)
			    + SHorizontalBox::Slot()
			    .Padding(0.0f, 0.0f, 2.0f, 0.0f)
			    .AutoWidth()
			    [
				    SAssignNew(DTThumbnailBorder, SBorder)
					    .Padding(5.0f)
					    .BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) DT)
					    .OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) DT)
				    [
					    SNew(SBox)
						    .WidthOverride(64)
						    .HeightOverride(64)
						    .ToolTipText(FText::FromString(DT->GetPathName()))
					    [
						    DTThumbnail->MakeThumbnailWidget()
					    ]
				    ]
			    ]
			    + SHorizontalBox::Slot()
			    .AutoWidth()
			    .Padding(2.0f, 0.0f)
			    .VAlign(VAlign_Center)
			    [
				    SNew(SVerticalBox)
				    + SVerticalBox::Slot()
				    .AutoHeight()
				    .Padding(2.0f, 0.0f)
				    .VAlign(VAlign_Center)
				    [
					    SNew(STextBlock)
					    .Text(FText::FromName(DT->GetFName()))
					    .Font(IDetailLayoutBuilder::GetDetailFont())
				    ]
				    + SVerticalBox::Slot()
				    .AutoHeight()
				    .Padding(2, 0)
				    .VAlign(VAlign_Center)
				    .HAlign(HAlign_Left)
				    [
					    PropertyCustomizationHelpers::MakeBrowseButton(
						    FSimpleDelegate::CreateSP(
							    this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&) DT),
						    TAttribute<FText>(LOCTEXT("HoudiniDataTableBrowseButton", "Browse to this generated Data Table in the content browser")))
				    ]
			    ]
		    ];

		    DTOutputGrp.AddWidgetRow()
		    .NameContent()
		    [
			    SNew(STextBlock)
			    .Text(FText::FromString("Row Struct"))
			    .Font(IDetailLayoutBuilder::GetDetailFont())
		    ]
		    .ValueContent()
		    .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		    [
			    RSVerticalBox
		    ];

		    RSVerticalBox->AddSlot()
		    .Padding(0, 2)
		    .AutoHeight()
		    [
			    SNew(SHorizontalBox)
			    + SHorizontalBox::Slot()
			    .Padding(0.0f, 0.0f, 2.0f, 0.0f)
			    .AutoWidth()
			    [
				    SAssignNew(RSThumbnailBorder, SBorder)
				    .Padding(5.0f)
				    .BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) DT->RowStruct)
				    .OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) DT->RowStruct)
				    [
					    SNew(SBox)
					    .WidthOverride(64)
					    .HeightOverride(64)
					    .ToolTipText(FText::FromName(DT->RowStruct->GetFName()))
					    [
						    RSThumbnail->MakeThumbnailWidget()
					    ]
				    ]
			    ]
			    + SHorizontalBox::Slot()
			    .AutoWidth()
			    .Padding(2.0f, 0.0f)
			    .VAlign(VAlign_Center)
			    [
				    SNew(SVerticalBox)
				    + SVerticalBox::Slot()
				    .AutoHeight()
				    .Padding(2.0f, 0.0f)
				    .VAlign(VAlign_Center)
				    [
					    SNew(STextBlock)
					    .Text(FText::FromName(DT->RowStruct->GetFName()))
					    .Font(IDetailLayoutBuilder::GetDetailFont())
				    ]
				    + SVerticalBox::Slot()
				    .AutoHeight()
				    .Padding(2.0f, 0.0f)
				    .VAlign(VAlign_Center)
				    .HAlign(HAlign_Left)
				    [
					    PropertyCustomizationHelpers::MakeBrowseButton(
						    FSimpleDelegate::CreateSP(
							    this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&) DT->RowStruct),
						    TAttribute<FText>(LOCTEXT("HoudiniDataTableRowStructBrowseButton", "Browse to the generated Data Table's row struct in the content browser")))
				    ]
			    ]
			    
		    ];

		    OutputObjectThumbnailBorders.Add((UObject*) DT, DTThumbnailBorder);
		    OutputObjectThumbnailBorders.Add((UObject*) DT->RowStruct, RSThumbnailBorder);
		}
	}
}

void FHoudiniOutputDetails::CreateLandscapeSplineOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory, const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	
	for (const auto& IterObject : OutputObjects)
	{
		const FHoudiniOutputObject& CurrentOutputObject = IterObject.Value;

		ALandscapeSplineActor* const LandscapeSplineActor = Cast<ALandscapeSplineActor>(IterObject.Value.OutputObject);

		if (!IsValid(LandscapeSplineActor))
			continue;

		const FHoudiniOutputObjectIdentifier& OutputIdentifier = IterObject.Key;
		FHoudiniGeoPartObject HoudiniGeoPartObject;
		for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects()) 
		{
			if (!OutputIdentifier.Matches(curHGPO))
				continue;

			HoudiniGeoPartObject = curHGPO;
			break;
		}

		CreateLandscapeSplineWidgets(
			HouOutputCategory,
			InOutput,
			LandscapeSplineActor,
			CurrentOutputObject,
			OutputIdentifier,
			HoudiniGeoPartObject);
	}
}

void FHoudiniOutputDetails::CreateLandscapeSplineWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput,
	const TWeakObjectPtr<ALandscapeSplineActor>& LandscapeSplineActor,
	const FHoudiniOutputObject& OutputObject,
	const FHoudiniOutputObjectIdentifier& OutputIdentifier,
	const FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	if (!IsValidWeakPointer(LandscapeSplineActor))
		return;

	ULandscapeSplinesComponent* const SplinesComponent = LandscapeSplineActor->GetSplinesComponent();
	if (!IsValid(SplinesComponent))
		return;

	// FHoudiniOutputObject const* const FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);

	// Get thumbnail pool for this builder.
	const IDetailLayoutBuilder& DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	const TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	const FString Label = HoudiniGeoPartObject.bHasCustomPartName ? HoudiniGeoPartObject.PartName : LandscapeSplineActor->GetName();

	// Create thumbnail for this mesh.
	const TSharedPtr<FAssetThumbnail> StaticMeshThumbnail =
		MakeShareable(new FAssetThumbnail(LandscapeSplineActor.Get(), 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> StaticMeshThumbnailBorder;

	const TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	
	IDetailGroup& StaticMeshGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));

	const FString LandscapeSplineLabel = TEXT("Landscape Spline");
	
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew( STextBlock )
		.Text( FText::FromString(LandscapeSplineLabel) )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];
			
	VerticalBox->AddSlot()
	.Padding( 0, 2 )
	.AutoHeight()
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		.AutoWidth()
		[
			SAssignNew( StaticMeshThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage( this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) LandscapeSplineActor )
			.OnMouseDoubleClick( this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) LandscapeSplineActor )
			[
				SNew( SBox )
				.WidthOverride( 64 )
				.HeightOverride( 64 )
				.ToolTipText( FText::FromString( SplinesComponent->GetPathName() ) )
				[
					StaticMeshThumbnail->MakeThumbnailWidget()
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&) LandscapeSplineActor),
				TAttribute<FText>(LOCTEXT("HoudiniLandscapeSplineActorBrowseButton", "Select this Landscape Spline Actor in the viewport")))
		]
	];

	OutputObjectThumbnailBorders.Add(LandscapeSplineActor, StaticMeshThumbnailBorder);	
}


void 
FHoudiniOutputDetails::CreateCurveWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput,
	const TWeakObjectPtr<USceneComponent>& SplineComponent,
	FHoudiniOutputObject& OutputObject,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	FHoudiniGeoPartObject& HoudiniGeoPartObject) 
{
	if (!IsValidWeakPointer(InOutput))
		return;

	// We support Unreal Spline out only for now
	const TWeakObjectPtr<USplineComponent>& SplineOutput = Cast<USplineComponent>(SplineComponent);
	if (!IsValidWeakPointer(SplineOutput))
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!IsValidWeakPointer(HAC))
		return;

	const TWeakObjectPtr<AActor>& OwnerActor = HAC->GetOwner();
	if (!IsValidWeakPointer(OwnerActor))
		return;

	FHoudiniCurveOutputProperties* OutputProperty = &(OutputObject.CurveOutputProperty);
	EHoudiniCurveType OutputCurveType = OutputObject.CurveOutputProperty.CurveType;

	FString Label = SplineComponent->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	//Label += FString("_") + OutputIdentifier.SplitIdentifier;

	FString OutputCurveName = OutputObject.BakeName;
	if(OutputCurveName.IsEmpty())
		OutputCurveName = OwnerActor->GetActorNameOrLabel() + "_" + Label;

	const FText& LabelText = FText::FromString("Unreal Spline");

	IDetailGroup& CurveOutputGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label), false, false);

	// Bake name row UI
	CurveOutputGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(OutputObject.BakeName))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.OnTextCommitted_Lambda([InOutput, OutputIdentifier](const FText& Val, ETextCommit::Type TextCommitType)
			{
				if (InOutput.IsValid())
				{
					FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
					FHoudiniEngineUtils::UpdateEditorProperties(true);
				}
			})
		]

		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]()
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];

	CurveOutputGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("OutputCurveSplineType", "Spline Type"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText_Lambda([SplineOutput, Label, OutputCurveType]()
		{
		FString ToolTipStr = FString::Printf(TEXT(" curve: %s\n Export type: Unreal Spline\n num points: %d\n curve type: %s\n closed: %s"),
			*Label,
			SplineOutput->GetNumberOfSplinePoints(),
			*FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(OutputCurveType),
			SplineOutput->IsClosedLoop() ? *(FString("yes")) : *(FString("no")));

		return FText::FromString(ToolTipStr);
	})
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(STextBlock)
		// We support Unreal Spline output only for now...
		.Text(LOCTEXT("OutputCurveSplineTypeUnreal", "Unreal Spline"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	//if (bIsUnrealSpline)
	//{
		const TWeakObjectPtr<USplineComponent>& UnrealSpline = Cast<USplineComponent>(SplineComponent);

		// Curve type combo box UI
		auto InitialSelectionLambda = [OutputProperty]()
		{
			if (OutputProperty->CurveType == EHoudiniCurveType::Polygon)
			{
				return (*FHoudiniEngineEditor::Get().GetUnrealOutputCurveTypeLabels())[0];
			}
			else
			{
				return (*FHoudiniEngineEditor::Get().GetUnrealOutputCurveTypeLabels())[1];
			}
		};

		TSharedPtr<SComboBox<TSharedPtr<FString>>> UnrealCurveTypeComboBox;

		CurveOutputGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("OutputCurveUnrealSplinePointType", "Spline Point Type"))
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SAssignNew(UnrealCurveTypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetUnrealOutputCurveTypeLabels())
			.InitiallySelectedItem(InitialSelectionLambda())
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > InItem)
		{
			return SNew(STextBlock).Text(FText::FromString(*InItem));
		})
		.OnSelectionChanged_Lambda(
			[OutputIdentifier, InOutput, SplineComponent](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
		{
			if (!IsValidWeakPointer(SplineComponent))
				return;
				
			// Set the curve point type locally
			USplineComponent* Spline = Cast<USplineComponent>(SplineComponent);
			if (!IsValid(Spline))
				return;

			FString *NewChoiceStr = NewChoice.Get();
			if (!NewChoiceStr)
				return;

			if (!IsValidWeakPointer(InOutput))
				return;

			FHoudiniOutputObject* const OutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
			if (!OutputObject)
				return;
				
			FHoudiniCurveOutputProperties& OutputProperty = OutputObject->CurveOutputProperty;

			if (*NewChoiceStr == "Linear")
			{
				if (OutputProperty.CurveType == EHoudiniCurveType::Polygon)
					return;

				OutputProperty.CurveType = EHoudiniCurveType::Polygon;

				for (int32 PtIdx = 0; PtIdx < Spline->GetNumberOfSplinePoints(); ++PtIdx)
				{
					Spline->SetSplinePointType(PtIdx, ESplinePointType::Linear);
				}

				FHoudiniEngineEditorUtils::ReselectSelectedActors();
				FHoudiniEngineUtils::UpdateEditorProperties(true);
			}
			else if (*NewChoiceStr == "Curve")
			{
				if (OutputProperty.CurveType != EHoudiniCurveType::Polygon)
					return;

				OutputProperty.CurveType = EHoudiniCurveType::Bezier;

				for (int32 PtIdx = 0; PtIdx < Spline->GetNumberOfSplinePoints(); ++PtIdx)
				{
					Spline->SetSplinePointType(PtIdx, ESplinePointType::Curve);
				}

				FHoudiniEngineEditorUtils::ReselectSelectedActors();
				FHoudiniEngineUtils::UpdateEditorProperties(true);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([OutputProperty]()
			{
				if (OutputProperty->CurveType == EHoudiniCurveType::Polygon)
					return FText::FromString(TEXT("Linear"));
				else
					return FText::FromString(TEXT("Curve"));
			})
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		// Add closed curve checkbox UI
		TSharedPtr<SCheckBox> ClosedCheckBox;
		CurveOutputGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("OutputCurveUnrealSplineClosed", "Closed"))
		]
		.ValueContent()
		[
			SAssignNew(ClosedCheckBox, SCheckBox)
			.OnCheckStateChanged_Lambda([UnrealSpline, InOutput](ECheckBoxState NewState)
			{
				if (!IsValidWeakPointer(UnrealSpline))
					return;

				UnrealSpline->SetClosedLoop(NewState == ECheckBoxState::Checked);
				FHoudiniEngineEditorUtils::ReselectSelectedActors();

				if (InOutput.IsValid())
					FHoudiniEngineUtils::UpdateEditorProperties(true);
			})
			.IsChecked_Lambda([UnrealSpline]()
			{
				if (!IsValidWeakPointer(UnrealSpline))
					return ECheckBoxState::Unchecked;

				return UnrealSpline->IsClosedLoop() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
		];
	//}

	// Add Bake Button UI
	TSharedPtr<SButton> BakeButton;
	CurveOutputGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
	]
	.ValueContent()
	[
		SAssignNew(BakeButton, SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("OutputCurveBakeButtonText", "Bake Output"))
		.IsEnabled(true)
		.ToolTipText(LOCTEXT("OutputCurveBakeButtonUnrealSplineTooltipText", "Bake this output curve to an Actor with a Spline Component."))
		.OnClicked_Lambda([InOutput, SplineComponent, OutputIdentifier, HAC, OutputCurveName]()
		{
			if (!HAC.IsValid() || !SplineComponent.IsValid() || !InOutput.IsValid())
				return FReply::Handled();

			FHoudiniOutputObject* const OutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
			if (OutputObject)
				return FReply::Handled();

			TArray<UHoudiniOutput*> AllOutputs;
			AllOutputs.Reserve(HAC->GetNumOutputs());
			HAC->GetOutputs(AllOutputs);

			FHoudiniGeoPartObject HoudiniGeoPartObject;
			for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects()) 
			{
				if (!OutputIdentifier.Matches(curHGPO))
					continue;

				HoudiniGeoPartObject = curHGPO;
				break;
			}

			FHoudiniBakeSettings BakeSettings;

			FHoudiniOutputDetails::OnBakeOutputObject(
				OutputCurveName,
				SplineComponent.Get(),
				OutputIdentifier,
				*OutputObject,
				HoudiniGeoPartObject,
				HAC.Get(),
				InOutput.Get(),
				HAC->BakeFolder.Path,
				BakeSettings,
				HAC->TemporaryCookFolder.Path,
				EHoudiniLandscapeOutputBakeType::InValid,
				AllOutputs);

			return FReply::Handled();
		})
	];
}


void FHoudiniOutputDetails::CreateGeometryCollectionWidgets(IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput, const TWeakObjectPtr<AGeometryCollectionActor>& GeometryCollectionActor,
	FHoudiniOutputObject& OutputObject, FHoudiniOutputObjectIdentifier& OutputIdentifier, FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	if (!IsValidWeakPointer(GeometryCollectionActor))
		return;
	
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionActor->GetGeometryCollectionComponent()->EditRestCollection(GeometryCollection::EEditUpdate::RestPhysicsDynamic);
	UGeometryCollection* GeometryCollection = GeometryCollectionEdit.GetRestCollection();
	
	if (!IsValid(GeometryCollection))
		return;

	FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	FString BakeName = FoundOutputObject ? FoundOutputObject->BakeName : FString();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// TODO: GetBakingBaseName!
	FString Label = GeometryCollection->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	int32 NumPieces = GeometryCollection->GeometrySource.Num();
	FString PiecesString = NumPieces > 1 ? FString::FromInt(NumPieces) + TEXT(" pieces.") : TEXT("Geometry Collection");

	// Create thumbnail for this mesh.
	TSharedPtr< FAssetThumbnail > StaticMeshThumbnail =
		MakeShareable(new FAssetThumbnail(GeometryCollection, 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> StaticMeshThumbnailBorder;

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	
	IDetailGroup& StaticMeshGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));

	FString GeometryCollectionLabel = TEXT( "Geometry Collection" );
	
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew( STextBlock )
		.Text( FText::FromString(GeometryCollectionLabel) )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];
			
	VerticalBox->AddSlot()
	.Padding( 0, 2 )
	.AutoHeight()
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		.AutoWidth()
		[
			SAssignNew( StaticMeshThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage( this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&)GeometryCollection )
			.OnMouseDoubleClick( this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) GeometryCollection )
			[
				SNew( SBox )
				.WidthOverride( 64 )
				.HeightOverride( 64 )
				.ToolTipText( FText::FromString( GeometryCollection->GetPathName() ) )
				[
					StaticMeshThumbnail->MakeThumbnailWidget()
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(PiecesString))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)GeometryCollection),
				TAttribute<FText>(LOCTEXT("HoudiniGeometryCollectionBrowseButton", "Browse to this generated Geometry Collection in the content browser")))
		]
	];

	OutputObjectThumbnailBorders.Add((UObject*)GeometryCollection, StaticMeshThumbnailBorder);
	
}

void
FHoudiniOutputDetails::CreateStaticMeshAndMaterialWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput,
	const TWeakObjectPtr<UStaticMesh>& StaticMesh,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	FHoudiniGeoPartObject& HoudiniGeoPartObject,
	const bool& bIsProxyMeshCurrent)
{
	if (!IsValidWeakPointer(StaticMesh))
		return;

	const TWeakObjectPtr<UHoudiniAssetComponent>& OwningHAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	
	FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	FString BakeName = FoundOutputObject ? FoundOutputObject->BakeName : FString();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// TODO: GetBakingBaseName!
	FString Label = StaticMesh->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	// Create thumbnail for this mesh.
	TSharedPtr< FAssetThumbnail > StaticMeshThumbnail =
		MakeShareable(new FAssetThumbnail(StaticMesh.Get(), 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> StaticMeshThumbnailBorder;

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	
	IDetailGroup& StaticMeshGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(BakeName))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OnTextCommitted_Lambda([OutputIdentifier, InOutput](const FText& Val, ETextCommit::Type TextCommitType)
			{
				if (!IsValidWeakPointer(InOutput))
					return;

				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(true);
			})
			.ToolTipText( LOCTEXT( "BakeNameTip", "The base name of the baked asset") )
		]

		+SHorizontalBox::Slot()
		.Padding( 2.0f, 0.0f )
		.VAlign( VAlign_Center )
		.AutoWidth()
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT( "RevertNameOverride", "Revert bake name override" ) )
			.ButtonStyle(_GetEditorStyle(), "NoBorder" )
			.ContentPadding( 0 )
			.Visibility( EVisibility::Visible )
			.OnClicked_Lambda([InOutput, OutputIdentifier]() 
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(true);
				return FReply::Handled();
			})
			[
				SNew( SImage )
				.Image(_GetEditorStyle().GetBrush( "PropertyWindow.DiffersFromDefault" ) )
			]
		]
	];

	// Add details on the SM colliders
	EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(OutputIdentifier.SplitIdentifier);
	FString MeshLabel = TEXT( "Static Mesh" );

	// If the Proxy mesh is more recent, indicate it in the details
	if (bIsProxyMeshCurrent)
	{
		MeshLabel += TEXT("\n(unrefined)");
	}

	// Indicate that this mesh is instanced
	if (HoudiniGeoPartObject.bIsInstanced)
	{
		MeshLabel += TEXT("\n(instanced)");
	}

	if (HoudiniGeoPartObject.bIsTemplated)
	{
		MeshLabel += TEXT("\n(templated)");
	}

	int32 NumSimpleColliders = 0;
	if (IsValid(StaticMesh->GetBodySetup()))
		NumSimpleColliders = StaticMesh->GetBodySetup()->AggGeom.GetElementCount();

	if(NumSimpleColliders > 0)
	{
		MeshLabel += TEXT( "\n(") + FString::FromInt(NumSimpleColliders) + TEXT(" Simple Collider" );
		if (NumSimpleColliders > 1 )
			MeshLabel += TEXT("s");
		MeshLabel += TEXT(")");
	}
	else if (SplitType == EHoudiniSplitType::RenderedComplexCollider)
	{
		MeshLabel += TEXT( "\n(Rendered Complex Collider)" );
	}
	else if(SplitType == EHoudiniSplitType::InvisibleComplexCollider )
	{
		MeshLabel += TEXT( "\n(Invisible Complex Collider)" );
	}

	if ( StaticMesh->GetNumLODs() > 1 )
		MeshLabel += TEXT("\n(") + FString::FromInt( StaticMesh->GetNumLODs() ) + TEXT(" LODs)");

	if (HoudiniGeoPartObject.AllMeshSockets.Num() > 0)
	{
		if (bIsProxyMeshCurrent)
		{
			// Proxy is current, show the number of sockets on the HGPO
			MeshLabel += TEXT("\n(") + FString::FromInt(HoudiniGeoPartObject.AllMeshSockets.Num()) + TEXT(" sockets)");
		}
		else
		{
			// Show the number of sockets on the SM
			MeshLabel += TEXT("\n(") + FString::FromInt(StaticMesh->Sockets.Num()) + TEXT(" sockets)");
		}
	}

	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew( STextBlock )
		.Text( FText::FromString(MeshLabel) )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];
			
	VerticalBox->AddSlot()
	.Padding( 0, 2 )
	.AutoHeight()
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		.AutoWidth()
		[
			SAssignNew( StaticMeshThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage( this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&)StaticMesh )
			.OnMouseDoubleClick( this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) StaticMesh )
			[
				SNew( SBox )
				.WidthOverride( 64 )
				.HeightOverride( 64 )
				.ToolTipText( FText::FromString( StaticMesh->GetPathName() ) )
				[
					StaticMeshThumbnail->MakeThumbnailWidget()
				]
			]
		]

		+SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		.Padding( 0.0f, 4.0f, 4.0f, 4.0f )
		.VAlign( VAlign_Center )
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.MaxWidth( 120.0f )
				[
					SNew( SButton )
					.VAlign( VAlign_Center )
					.HAlign( HAlign_Center )
					.Text( LOCTEXT( "BakeOutputMesh", "Bake Output" ) )
					.IsEnabled(true)
					.OnClicked_Lambda([BakeName, StaticMesh, OutputIdentifier, InOutput, OwningHAC]()
					{
						if (!StaticMesh.IsValid() || !InOutput.IsValid())
							return FReply::Handled();
						
						FHoudiniOutputObject* const FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
						if (!FoundOutputObject)
							return FReply::Handled();

						TArray<UHoudiniOutput*> AllOutputs;
						FString TempCookFolder;
						FString BakeFolder;
						if (OwningHAC.IsValid())
						{
							AllOutputs.Reserve(OwningHAC->GetNumOutputs());
							OwningHAC->GetOutputs(AllOutputs);

							TempCookFolder = OwningHAC->TemporaryCookFolder.Path;
							BakeFolder = OwningHAC->BakeFolder.Path;
						}

						FHoudiniBakeSettings BakeSettings;
						BakeSettings.SetFromHAC(OwningHAC.Get());

						FHoudiniGeoPartObject HoudiniGeoPartObject;
						for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects())
						{
							if (!OutputIdentifier.Matches(curHGPO))
								continue;

							HoudiniGeoPartObject = curHGPO;
							break;
						}

						FHoudiniOutputDetails::OnBakeOutputObject(
							BakeName,
							StaticMesh.Get(),
							OutputIdentifier,
							*FoundOutputObject,
							HoudiniGeoPartObject,
							OwningHAC.Get(),
							InOutput.Get(),
							BakeFolder,
							BakeSettings,
							TempCookFolder,
							EHoudiniLandscapeOutputBakeType::InValid,
							AllOutputs);

						return FReply::Handled();
					})
					.ToolTipText( LOCTEXT( "HoudiniStaticMeshBakeButton", "Bake this output object to a Static Mesh in the Bake folder." ) )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateSP(
							this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)StaticMesh),
							TAttribute<FText>(LOCTEXT("HoudiniStaticMeshBrowseButton", "Browse to this generated static mesh in the content browser")))
				]
			]
		]
	];

	// Store thumbnail for this mesh.
	OutputObjectThumbnailBorders.Add(StaticMesh, StaticMeshThumbnailBorder);

	// We need to add material box for each material present in this static mesh.
	auto & StaticMeshMaterials = StaticMesh->GetStaticMaterials();
	for ( int32 MaterialIdx = 0; MaterialIdx < StaticMeshMaterials.Num(); ++MaterialIdx )
	{
		UMaterialInterface * MaterialInterface = StaticMeshMaterials[ MaterialIdx ].MaterialInterface;
		TSharedPtr< SBorder > MaterialThumbnailBorder;
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if ( IsValid(MaterialInterface)
			&& IsValid(MaterialInterface->GetOuter()) )
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}
		else
		{
			MaterialInterface = nullptr;
			MaterialName = TEXT("Material (invalid)") + FString::FromInt( MaterialIdx ) ;
			MaterialPathName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
		}

		// Create thumbnail for this material.
		TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
			MakeShareable( new FAssetThumbnail( MaterialInterface, 64, 64, AssetThumbnailPool ) );

		VerticalBox->AddSlot().Padding( 0, 2 )
		[
			SNew( SAssetDropTarget )
			.OnAreAssetsAcceptableForDrop( this, &FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver )
			.OnAssetsDropped(
				this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, StaticMesh, InOutput, MaterialIdx )
			[
				SAssignNew( HorizontalBox, SHorizontalBox )
			]
		];

		HorizontalBox->AddSlot().Padding( 0.0f, 0.0f, 2.0f, 0.0f ).AutoWidth()
		[
			SAssignNew( MaterialThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage(
				this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (const TWeakObjectPtr<UObject>&)StaticMesh, MaterialIdx )
			.OnMouseDoubleClick(
				this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&)MaterialInterface )
			[
				SNew( SBox )
				.WidthOverride( 64 )
				.HeightOverride( 64 )
				.ToolTipText( FText::FromString( MaterialPathName ) )
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this mesh and material index.
		{
			TPairInitializer<const TWeakObjectPtr<UStaticMesh>&, int32> Pair( StaticMesh, MaterialIdx );
			MaterialInterfaceThumbnailBorders.Add( Pair, MaterialThumbnailBorder );
		}

		// ComboBox and buttons
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Add Combo box
		TSharedPtr< SComboButton > AssetComboButton;
		ComboAndButtonBox->AddSlot().VAlign(VAlign_Center).FillHeight(1.0f)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().VAlign(VAlign_Center).FillHeight(1.0f)
			[
				SAssignNew(AssetComboButton, SComboButton)
				.ButtonStyle(_GetEditorStyle(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(_GetEditorStyle().GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent,
				TWeakObjectPtr<UMaterialInterface>(MaterialInterface), (const TWeakObjectPtr<UObject>&)StaticMesh, InOutput, MaterialIdx)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
					.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];


		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);


		// Add buttons
		TSharedPtr< SHorizontalBox > ButtonBox;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];

		// Use CB selection arrow button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(const TWeakObjectPtr<UObject>&)StaticMesh, InOutput, MaterialIdx),
				TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")))
		];

		// Browse CB button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign( VAlign_Center )
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)MaterialInterface ), TAttribute< FText >( MaterialTooltip ) )
		];

		// Reset button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign( VAlign_Center )
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT( "ResetToBaseMaterial", "Reset to base material" ) )
			.ButtonStyle(_GetEditorStyle(), "NoBorder" )
			.ContentPadding( 0 )
			.Visibility( EVisibility::Visible )
			.OnClicked(
				this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, StaticMesh, InOutput, MaterialIdx)
			[
				SNew( SImage )
				.Image(_GetEditorStyle().GetBrush( "PropertyWindow.DiffersFromDefault" ) )
			]
		];

		// Store combo button for this mesh and index.
		{
			TPairInitializer<const TWeakObjectPtr<UStaticMesh>&, int32> Pair( StaticMesh, MaterialIdx );
			MaterialInterfaceComboButtons.Add( Pair, AssetComboButton );
		}
	}
}

void
FHoudiniOutputDetails::CreateProxyMeshAndMaterialWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput,
	const TWeakObjectPtr<UHoudiniStaticMesh>& ProxyMesh,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	if (!IsValidWeakPointer(ProxyMesh))
		return;

	FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	FString BakeName = FoundOutputObject ? FoundOutputObject->BakeName : FString();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// TODO: GetBakingBaseName!
	FString Label = ProxyMesh->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	// Create thumbnail for this mesh.
	TSharedPtr<FAssetThumbnail> MeshThumbnail =	MakeShareable(new FAssetThumbnail(ProxyMesh.Get(), 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> MeshThumbnailBorder;

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	IDetailGroup& StaticMeshGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));

	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(BakeName))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OnTextCommitted_Lambda([OutputIdentifier, InOutput](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(true);
			})
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
		]
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]()
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(true);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];

	// Add details on the Proxy Mesh
	EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(OutputIdentifier.SplitIdentifier);
	FString MeshLabel = TEXT("Proxy Mesh");

	// Indicate that this mesh is instanced
	if (HoudiniGeoPartObject.bIsInstanced)
	{
		MeshLabel += TEXT("\n(instanced)");
	}

	if (HoudiniGeoPartObject.bIsTemplated)
	{
		MeshLabel += TEXT("\n(templated)");
	}

	// Mention complex colliders even if they are not used for proxies
	if (SplitType == EHoudiniSplitType::RenderedComplexCollider)
	{
		MeshLabel += TEXT("\n(Rendered Complex Collider)");
	}
	else if (SplitType == EHoudiniSplitType::InvisibleComplexCollider)
	{
		MeshLabel += TEXT("\n(Invisible Complex Collider)");
	}

	if (HoudiniGeoPartObject.AllMeshSockets.Num() > 0)
	{
		MeshLabel += TEXT("\n(") + FString::FromInt(HoudiniGeoPartObject.AllMeshSockets.Num()) + TEXT(" sockets)");
	}

	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(MeshLabel))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];

	VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.AutoWidth()
		[
			SAssignNew(MeshThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&) ProxyMesh)
			.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) ProxyMesh)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(ProxyMesh->GetPathName()))
				[
					MeshThumbnail->MakeThumbnailWidget()
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				//.MaxWidth(80.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("RefineOutputProxy", "Refine Proxy"))
					.IsEnabled(true)
					.OnClicked(this, &FHoudiniOutputDetails::OnRefineClicked, (const TWeakObjectPtr<UObject>&)ProxyMesh, InOutput)
					.ToolTipText(LOCTEXT("RefineTooltip", "Refine this Proxy Mesh Component to a Static Mesh Component."))
				]
			]
		]
	];

	// Store thumbnail for this mesh.
	OutputObjectThumbnailBorders.Add(ProxyMesh, MeshThumbnailBorder);

	// We need to add material box for each material present in this static mesh.
	auto & ProxyMeshMaterials = ProxyMesh->GetStaticMaterials();
	for (int32 MaterialIdx = 0; MaterialIdx < ProxyMeshMaterials.Num(); ++MaterialIdx)
	{
		UMaterialInterface * MaterialInterface = ProxyMeshMaterials[MaterialIdx].MaterialInterface;
		TSharedPtr< SBorder > MaterialThumbnailBorder;
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if (IsValid(MaterialInterface)
			&& IsValid(MaterialInterface->GetOuter()))
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}
		else
		{
			MaterialInterface = nullptr;
			MaterialName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
			MaterialPathName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
		}

		// Create thumbnail for this material.
		TSharedPtr<FAssetThumbnail> MaterialInterfaceThumbnail =
			MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

		// No drop target
		VerticalBox->AddSlot()
		.Padding(0, 2)
		[
			SNew(SAssetDropTarget)
			//.OnIsAssetAcceptableForDrop(false)
			//.OnAssetDropped(
			//	this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, StaticMesh, InOutput, MaterialIdx)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.AutoWidth()
		[
			SAssignNew(MaterialThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(
				this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (const TWeakObjectPtr<UObject>&) ProxyMesh, MaterialIdx)
			.OnMouseDoubleClick(
				this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) MaterialInterface)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(MaterialPathName))
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this mesh and material index.
		{
			TPairInitializer<const TWeakObjectPtr<UObject>&, int32> Pair(ProxyMesh, MaterialIdx);
			MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
		}
				
		// Combo box and buttons
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Add combo box
		TSharedPtr<SComboButton> AssetComboButton;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(AssetComboButton, SComboButton)
				.ButtonStyle(_GetEditorStyle(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(_GetEditorStyle().GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				/*.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent,
				MaterialInterface, StaticMesh, InOutput, MaterialIdx)*/
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
					.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];


		TSharedPtr<SHorizontalBox> ButtonBox;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];
		
		// Disable the combobutton for proxies
		AssetComboButton->SetEnabled(false);

		// Add use selection form content browser array
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				/*FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(UObject*)ProxyMesh, InOutput, MaterialIdx),*/
					FSimpleDelegate::CreateLambda([]() {}), // Do nothing for proxies
					TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")), false)
			// Disable the use CB selection button for proxies
		];
		

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&) MaterialInterface), TAttribute<FText>(MaterialTooltip))
		];

		/*
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("ResetToBaseMaterial", "Reset to base material"))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked(
				this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, StaticMesh, InOutput, MaterialIdx)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];
		*/

		// Store combo button for this mesh and index.
		{
			TPairInitializer<const TWeakObjectPtr<UObject>&, int32> Pair(ProxyMesh, MaterialIdx);
			MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
		}
	}
}

FText
FHoudiniOutputDetails::GetOutputDebugName(const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!InOutput.IsValid())
		return FText::FromString(TEXT("Invalid"));
	
	// Get the name and type
	FString OutputNameStr = InOutput->GetName() + TEXT(" ") + UHoudiniOutput::OutputTypeToString(InOutput->GetType());

	// Then add the number of parts		
	OutputNameStr += TEXT(" (") + FString::FromInt(InOutput->GetHoudiniGeoPartObjects().Num()) + TEXT(" Part(s))\n");

	return FText::FromString(OutputNameStr);
}
FText
FHoudiniOutputDetails::GetOutputDebugDescription(const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{	
	if (!InOutput.IsValid())
		return FText::FromString(TEXT("Invalid"));

	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();
	
	FString OutputValStr;
	OutputValStr += TEXT("HGPOs:\n");
	for (auto& HGPO : HGPOs)
	{
		OutputValStr += TEXT(" - ") + HGPO.PartName + TEXT(" (") + FHoudiniGeoPartObject::HoudiniPartTypeToString(HGPO.Type) + TEXT(")");

		if (HGPO.SplitGroups.Num() > 0)
		{
			OutputValStr += TEXT("( ") + FString::FromInt(HGPO.SplitGroups.Num()) + TEXT(" splits:");
			for (auto& split : HGPO.SplitGroups)
			{
				OutputValStr += TEXT(" ") + split;
			}
			OutputValStr += TEXT(")");
		}

		if (!HGPO.VolumeName.IsEmpty())
		{
			OutputValStr += TEXT("( ") + HGPO.VolumeName;
			if (HGPO.VolumeTileIndex >= 0)
				OutputValStr += TEXT(" tile ") + FString::FromInt(HGPO.VolumeTileIndex);
			OutputValStr += TEXT(" )");
		}

		OutputValStr += TEXT("\n");
	}

	// Add output objects if any
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> AllOutputObj = InOutput->GetOutputObjects();
	if (AllOutputObj.Num() > 0)
	{
		bool TitleAdded = false;
		for (const auto& Iter : AllOutputObj)
		{
			UObject* OutObject = Iter.Value.OutputObject;
			if (OutObject)
			{
				OutputValStr += OutObject->GetFullName() + TEXT(" (obj)\n");
			}

			for(auto Component : Iter.Value.OutputComponents)
			{
			    UObject* OutComp = Component;
			    if (OutComp)
			    {
				    OutputValStr += OutComp->GetFullName() + TEXT(" (comp)\n");
			    }
            }
		}
	}

	return FText::FromString(OutputValStr);
}

FText
FHoudiniOutputDetails::GetOutputTooltip(const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	// TODO
	return FText();
}


const FSlateBrush *
FHoudiniOutputDetails::GetThumbnailBorder(const TWeakObjectPtr<UObject> Mesh) const
{
	TSharedPtr<SBorder> ThumbnailBorder = OutputObjectThumbnailBorders[Mesh];
	if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
		return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailShadow");
}


const FSlateBrush *
FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder(const TWeakObjectPtr<UObject> OutputObject, int32 MaterialIdx) const
{
	if (!IsValidWeakPointer(OutputObject))
		return nullptr;

	TPairInitializer<const TWeakObjectPtr<UObject>&, int32> Pair(OutputObject, MaterialIdx);
	TSharedPtr<SBorder> ThumbnailBorder = MaterialInterfaceThumbnailBorders[Pair];

	if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
		return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailShadow");
}

/*
const FSlateBrush *
FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder(ALandscapeProxy * Landscape, int32 MaterialIdx) const
{
	if (!Landscape)
		return nullptr;

	TPairInitializer< ALandscapeProxy *, int32 > Pair(Landscape, MaterialIdx);
	TSharedPtr< SBorder > ThumbnailBorder = LandscapeMaterialInterfaceThumbnailBorders[Pair];

	if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}
*/

FReply
FHoudiniOutputDetails::OnThumbnailDoubleClick(
	const FGeometry & InMyGeometry,
	const FPointerEvent & InMouseEvent, const TWeakObjectPtr<UObject> Object)
{
	if (Object.IsValid() && GEditor)
		GEditor->EditObject(Object.Get());

	return FReply::Handled();
}

/*
FReply
FHoudiniOutputDetails::OnBakeStaticMesh(UStaticMesh * StaticMesh, UHoudiniAssetComponent * HoudiniAssetComponent, FHoudiniGeoPartObject& GeoPartObject)
{
	if (IsValid(HoudiniAssetComponent) && IsValid(StaticMesh))
	{
		FHoudiniPackageParams PackageParms;

		FHoudiniEngineBakeUtils::BakeStaticMesh(HoudiniAssetComponent, GeoPartObject, StaticMesh, PackageParms);
		// TODO: Bake the SM
		
		// We need to locate corresponding geo part object in component.
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = HoudiniAssetComponent->LocateGeoPartObject(StaticMesh);

		// (void)FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
		//	StaticMesh, HoudiniAssetComponent, HoudiniGeoPartObject, EBakeMode::ReplaceExisitingAssets);		
	}

	return FReply::Handled();
}
*/

bool
FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver(TArrayView<FAssetData> InAssets) const
{
	UObject* InObject = InAssets[0].GetAsset();
	return (InObject && InObject->IsA(UMaterialInterface::StaticClass()));
}


FReply
FHoudiniOutputDetails::OnResetMaterialInterfaceClicked(
	const TWeakObjectPtr<UStaticMesh> StaticMesh,
	const TWeakObjectPtr<UHoudiniOutput> HoudiniOutput,
	int32 MaterialIdx)
{
	FReply RetValue = FReply::Handled();
	if (!StaticMesh.IsValid() || !HoudiniOutput.IsValid())
		return RetValue;

	TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

	if (!StaticMaterials.IsValidIndex(MaterialIdx))
		return RetValue;

	// Retrieve material interface which is being replaced.
	UMaterialInterface * MaterialInterface = StaticMaterials[MaterialIdx].MaterialInterface;
	if (!MaterialInterface)
		return RetValue;

	// Find the string corresponding to the material that is being replaced	
	const FHoudiniMaterialIdentifier* MaterialIdentifierPtr = HoudiniOutput->GetReplacementMaterials().FindKey(MaterialInterface);
	if (!MaterialIdentifierPtr )
	{
		// This material was not replaced, no need to reset it
		return RetValue;
	}

	// This material has been replaced previously.
	const FHoudiniMaterialIdentifier MaterialIdentifier = *MaterialIdentifierPtr;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Reset"), HoudiniOutput.Get());

	// Remove the replacement
	HoudiniOutput->Modify();
	HoudiniOutput->GetReplacementMaterials().Remove(MaterialIdentifier);

	bool bViewportNeedsUpdate = true;

	// Try to find the original assignment, if not, we'll use the default material
	UMaterialInterface * AssignMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	UMaterialInterface * const * FoundMat = HoudiniOutput->GetAssignementMaterials().Find(MaterialIdentifier);
	if (FoundMat && (*FoundMat))
		AssignMaterial = *FoundMat;

	// Replace material on static mesh.
	StaticMesh->Modify();
	StaticMaterials[MaterialIdx].MaterialInterface = AssignMaterial;

	// Replace the material on any component (SMC/ISMC) that uses the above SM
	// TODO: ?? Replace for all?
	for (auto& OutputObject : HoudiniOutput->GetOutputObjects())
	{
		for(auto Component : OutputObject.Value.OutputComponents)
		{
		    // Only look at MeshComponents
		    UStaticMeshComponent * SMC = Cast<UStaticMeshComponent>(Component);
		    if (!SMC)
			    continue;

		    if (SMC->GetStaticMesh() != StaticMesh.Get())
			    continue;

		    SMC->Modify();
		    SMC->SetMaterial(MaterialIdx, AssignMaterial);
		}
	}

	FHoudiniEngineUtils::UpdateEditorProperties(true);

	if (GEditor)
		GEditor->RedrawAllViewports();

	return RetValue;
}

FReply
FHoudiniOutputDetails::OnResetMaterialInterfaceClicked(
	const TWeakObjectPtr<ALandscapeProxy> InLandscape,
	const TWeakObjectPtr<UHoudiniOutput> InHoudiniOutput,
	int32 InMaterialIdx)
{
	FReply RetValue = FReply::Handled();
	if (!InLandscape.IsValid() || !InHoudiniOutput.IsValid())
		return RetValue;
	
	// Retrieve the material interface which is being replaced.
	UMaterialInterface * MaterialInterface = InMaterialIdx == 0 ? InLandscape->GetLandscapeMaterial() : InLandscape->GetLandscapeHoleMaterial();
	UMaterialInterface * MaterialInterfaceReplacement = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

	// Find the string corresponding to the material that is being replaced	
	const FHoudiniMaterialIdentifier* MaterialIdentifierPtr = InHoudiniOutput->GetReplacementMaterials().FindKey(MaterialInterface);
	if (!MaterialIdentifierPtr)
	{
		// This material was not replaced, no need to reset it
		return RetValue;
	}

	// This material has been replaced previously.
	const FHoudiniMaterialIdentifier MaterialIdentifier = *MaterialIdentifierPtr;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Reset"), InHoudiniOutput.Get());

	// Remove the replacement
	InHoudiniOutput->Modify();
	InHoudiniOutput->GetReplacementMaterials().Remove(MaterialIdentifier);

	bool bViewportNeedsUpdate = true;

	// Try to find the original assignment, if not, we'll use the default material
	UMaterialInterface * AssignMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	UMaterialInterface * const * FoundMat = InHoudiniOutput->GetAssignementMaterials().Find(MaterialIdentifier);
	if (FoundMat && (*FoundMat))
		AssignMaterial = *FoundMat;

	// Replace material on Landscape
	InLandscape->Modify();
	if (InMaterialIdx == 0)
		InLandscape->LandscapeMaterial = AssignMaterial;
	else
		InLandscape->LandscapeHoleMaterial = AssignMaterial;
	
	InLandscape->UpdateAllComponentMaterialInstances();

	/*
	// As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
	// to trigger a fake Property change event that will call the Update function...
	UProperty* FoundProperty = FindField< UProperty >(Landscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
	if (FoundProperty)
	{
		FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
		Landscape->PostEditChangeProperty(PropChanged);
	}
	else
	{
		// The only way to update the material for now is to recook/recreate the landscape...
		HoudiniAssetComponent->StartTaskAssetCookingManual();
	}
	*/

	FHoudiniEngineUtils::UpdateEditorProperties(true);

	if (GEditor)
		GEditor->RedrawAllViewports();

	return RetValue;
}
/*
FReply
FHoudiniOutputDetails::OnResetMaterialInterfaceClicked(
	ALandscapeProxy * Landscape, UHoudiniOutput * InOutput, int32 MaterialIdx)
{
	bool bViewportNeedsUpdate = false;

	// TODO: Handle me!
	for (TArray< UHoudiniAssetComponent * >::TIterator
		IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
		if (!HoudiniAssetComponent)
			continue;

		TWeakObjectPtr<ALandscapeProxy>* FoundLandscapePtr = HoudiniAssetComponent->LandscapeComponents.Find(*HoudiniGeoPartObject);
		if (!FoundLandscapePtr)
			continue;

		ALandscapeProxy* FoundLandscape = FoundLandscapePtr->Get();
		if (!FoundLandscape || !FoundLandscape->IsValidLowLevel())
			continue;

		if (FoundLandscape != Landscape)
			continue;

		// Retrieve the material interface which is being replaced.
		UMaterialInterface * MaterialInterface = MaterialIdx == 0 ? Landscape->GetLandscapeMaterial() : Landscape->GetLandscapeHoleMaterial();
		UMaterialInterface * MaterialInterfaceReplacement = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

		bool bMaterialRestored = false;
		FString MaterialShopName;
		if (!HoudiniAssetComponent->GetReplacementMaterialShopName(*HoudiniGeoPartObject, MaterialInterface, MaterialShopName))
		{
			// This material was not replaced so there's no need to reset it
			continue;
		}

		// Remove the replacement
		HoudiniAssetComponent->RemoveReplacementMaterial(*HoudiniGeoPartObject, MaterialShopName);

		// Try to find the original assignment, if not, we'll use the default material
		UMaterialInterface * AssignedMaterial = HoudiniAssetComponent->GetAssignmentMaterial(MaterialShopName);
		if (AssignedMaterial)
			MaterialInterfaceReplacement = AssignedMaterial;

		// Replace material on the landscape
		Landscape->Modify();

		if (MaterialIdx == 0)
			Landscape->LandscapeMaterial = MaterialInterfaceReplacement;
		else
			Landscape->LandscapeHoleMaterial = MaterialInterfaceReplacement;

		//Landscape->UpdateAllComponentMaterialInstances();

		// As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
		// to trigger a fake Property change event that will call the Update function...
		UProperty* FoundProperty = FindField< UProperty >(Landscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
		if (FoundProperty)
		{
			FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
			Landscape->PostEditChangeProperty(PropChanged);
		}
		else
		{
			// The only way to update the material for now is to recook/recreate the landscape...
			HoudiniAssetComponent->StartTaskAssetCookingManual();
		}

		HoudiniAssetComponent->UpdateEditorProperties(false);
		bViewportNeedsUpdate = true;
	}

	if (GEditor && bViewportNeedsUpdate)
	{
		GEditor->RedrawAllViewports();
	}

	return FReply::Handled();
}
*/

void
FHoudiniOutputDetails::OnBrowseTo(const TWeakObjectPtr<UObject> InObject)
{
	if (!IsValidWeakPointer(InObject))
		return;
	
	if (GEditor)
	{
		TArray<UObject *> Objects;
		Objects.Add(InObject.Get());
		GEditor->SyncBrowserToObjects(Objects);
	}
}

TSharedRef<SWidget>
FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent(
	const TWeakObjectPtr<UMaterialInterface> MaterialInterface,
	const TWeakObjectPtr<UObject> OutputObject,
	const TWeakObjectPtr<UHoudiniOutput> InOutput,
	int32 MaterialIdx)
{
	TArray<const UClass *> AllowedClasses;
	AllowedClasses.Add(UMaterialInterface::StaticClass());

	TArray<UFactory *> NewAssetFactories;

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(MaterialInterface.Get()),
		true,
		AllowedClasses,
		NewAssetFactories,
		OnShouldFilterMaterialInterface,
		FOnAssetSelected::CreateSP(
			this, &FHoudiniOutputDetails::OnMaterialInterfaceSelected, OutputObject, InOutput, MaterialIdx),
		FSimpleDelegate::CreateSP(
			this, &FHoudiniOutputDetails::CloseMaterialInterfaceComboButton));
}


void
FHoudiniOutputDetails::CloseMaterialInterfaceComboButton()
{

}

void
FHoudiniOutputDetails::OnMaterialInterfaceDropped(
	const FDragDropEvent& InDragDropEvent,
	TArrayView<FAssetData> InAssets,
	const TWeakObjectPtr<UStaticMesh> StaticMesh,
	const TWeakObjectPtr<UHoudiniOutput> HoudiniOutput,
	int32 MaterialIdx)
{
	UObject* InObject = InAssets[0].GetAsset();
	if (!InObject)
		return;

	return OnMaterialInterfaceDropped(InObject, StaticMesh, HoudiniOutput, MaterialIdx);
}

void
FHoudiniOutputDetails::OnMaterialInterfaceDropped(
	const TWeakObjectPtr<UObject> InObject,
	const TWeakObjectPtr<UStaticMesh> StaticMesh,
	const TWeakObjectPtr<UHoudiniOutput> HoudiniOutput,
	int32 MaterialIdx)
{
	if (!InObject.IsValid() || !HoudiniOutput.IsValid())
		return;
	
	UMaterialInterface * MaterialInterface = Cast<UMaterialInterface>(InObject);
	if (!IsValid(MaterialInterface))
		return;

	if (!IsValidWeakPointer(StaticMesh))
		return;

	TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
	if (!StaticMaterials.IsValidIndex(MaterialIdx))
		return;

	bool bViewportNeedsUpdate = false;

	// Retrieve material interface which is being replaced.
	UMaterialInterface * OldMaterialInterface = StaticMaterials[MaterialIdx].MaterialInterface;
	if (OldMaterialInterface == MaterialInterface)
		return;

	// Find the string corresponding to the material that is being replaced
	FHoudiniMaterialIdentifier MaterialIdentifier;
	const FHoudiniMaterialIdentifier* MaterialIdentifierPtr = HoudiniOutput->GetReplacementMaterials().FindKey(OldMaterialInterface);
	if (MaterialIdentifierPtr)
	{
		// This material has been replaced previously.
		MaterialIdentifier = *MaterialIdentifierPtr;
	}
	else
	{
		// We have no previous replacement for this material,
		// see if we can find it the material assignment list.
		MaterialIdentifierPtr = HoudiniOutput->GetAssignementMaterials().FindKey(OldMaterialInterface);
		if (MaterialIdentifierPtr)
		{
			// This material has been assigned previously.
			MaterialIdentifier = *MaterialIdentifierPtr;
		}
		else
		{
			UMaterialInterface * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
			if (OldMaterialInterface == DefaultMaterial)
			{
				// This is replacement for default material.
				MaterialIdentifier = FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, "");
			}
			else
			{
				// External Material?
				// TODO: can we somehow map the material instance parameters here?
				if (IsValid(OldMaterialInterface))
				{
					MaterialIdentifier = FHoudiniMaterialIdentifier(OldMaterialInterface->GetPathName(), false, "");
				}
			}
		}
	}

	if (!MaterialIdentifier.IsValid())
		return;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Replacement"), HoudiniOutput.Get());

	// Add a new material replacement entry.
	HoudiniOutput->Modify(); 
	HoudiniOutput->GetReplacementMaterials().Add(MaterialIdentifier, MaterialInterface);	

	// Replace material on static mesh.
	StaticMesh->Modify();
	StaticMaterials[MaterialIdx].MaterialInterface = MaterialInterface;

	// Replace the material on any component (SMC/ISMC) that uses the above SM
	for (auto& OutputObject : HoudiniOutput->GetOutputObjects())
	{
		for(auto Component : OutputObject.Value.OutputComponents)
		{
		    // Only look at MeshComponents
		    UStaticMeshComponent * SMC = Cast<UStaticMeshComponent>(Component);
		    if (IsValid(SMC))
		    {
			    if (SMC->GetStaticMesh() == StaticMesh.Get())
			    {
				    SMC->Modify();
				    SMC->SetMaterial(MaterialIdx, MaterialInterface);
			    }
		    }
		    else 
		    {
			    UStaticMesh* SM = Cast<UStaticMesh>(OutputObject.Value.OutputObject);
			    if (IsValid(SM)) 
			    {
				    SM->Modify();
				    SM->SetMaterial(MaterialIdx, MaterialInterface);
			    }
			}
		}
	}

	FHoudiniEngineUtils::UpdateEditorProperties(true);

	/*
	if(GUnrealEd)
		GUnrealEd->UpdateFloatingPropertyWindows();
*/
	if (GEditor)
		GEditor->RedrawAllViewports();
}

// Delegate used when a valid material has been drag and dropped on a landscape.
void
FHoudiniOutputDetails::OnMaterialInterfaceDropped(
	const FDragDropEvent& InDragDropEvent,
	TArrayView<FAssetData> InAssets,
	const TWeakObjectPtr<ALandscapeProxy> InLandscape,
	const TWeakObjectPtr<UHoudiniOutput> InOutput,
	int32 MaterialIdx)
{
	UObject* InDroppedObject = InAssets[0].GetAsset();
	if (!InDroppedObject)
		return;

	return OnMaterialInterfaceDropped(InDroppedObject, InLandscape, InOutput, MaterialIdx);
}

// Delegate used when a valid material has been drag and dropped on a landscape.
void
FHoudiniOutputDetails::OnMaterialInterfaceDropped(
	UObject* InDroppedObject,
	const TWeakObjectPtr<ALandscapeProxy> InLandscape,
	const TWeakObjectPtr<UHoudiniOutput> InOutput,
	int32 MaterialIdx)
{
	UMaterialInterface * MaterialInterface = Cast< UMaterialInterface >(InDroppedObject);
	if (!IsValid(MaterialInterface))
		return;

	if (!InLandscape.IsValid() || !InOutput.IsValid())
		return;

	bool bViewportNeedsUpdate = false;

	// Retrieve the material interface which is being replaced.
	UMaterialInterface * OldMaterialInterface = MaterialIdx == 0 ? InLandscape->GetLandscapeMaterial() : InLandscape->GetLandscapeHoleMaterial();
	if (OldMaterialInterface == MaterialInterface)
		return;

	// Find the string corresponding to the material that is being replaced
	FHoudiniMaterialIdentifier MaterialIdentifier;
	const FHoudiniMaterialIdentifier* MaterialIdentifierPtr = InOutput->GetReplacementMaterials().FindKey(OldMaterialInterface);
	if (MaterialIdentifierPtr)
	{
		// This material has been replaced previously.
		MaterialIdentifier = *MaterialIdentifierPtr;
	}
	else
	{
		// We have no previous replacement for this material,
		// see if we can find it the material assignment list.
		MaterialIdentifierPtr = InOutput->GetAssignementMaterials().FindKey(OldMaterialInterface);
		if (MaterialIdentifierPtr)
		{
			// This material has been assigned previously.
			MaterialIdentifier = *MaterialIdentifierPtr;
		}
		else
		{
			UMaterialInterface * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
			if (OldMaterialInterface == DefaultMaterial)
			{
				// This is replacement for default material.
				MaterialIdentifier = FHoudiniMaterialIdentifier(HAPI_UNREAL_DEFAULT_MATERIAL_NAME, false, "");
			}
			else
			{
				// External Material?
				if (IsValid(OldMaterialInterface))
				{
					MaterialIdentifier = FHoudiniMaterialIdentifier(
						OldMaterialInterface->GetPathName(), false, "");
				}
			}
		}
	}

	if (!MaterialIdentifier.IsValid())
		return;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Replacement"), InOutput.Get());

	// Add a new material replacement entry.
	InOutput->Modify();
	InOutput->GetReplacementMaterials().Add(MaterialIdentifier, MaterialInterface);

	// Replace material on the landscape
	InLandscape->Modify();

	if (MaterialIdx == 0)
		InLandscape->LandscapeMaterial = MaterialInterface;
	else
		InLandscape->LandscapeHoleMaterial = MaterialInterface;

	// Update the landscape components Material instances
	InLandscape->UpdateAllComponentMaterialInstances();
	
	/*
	// As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
	// to trigger a fake Property change event that will call the Update function...
	UProperty* FoundProperty = FindField< UProperty >(InLandscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
	if (FoundProperty)
	{
		FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
		InLandscape->PostEditChangeProperty(PropChanged);
	}
	else
	{
		// The only way to update the material for now is to recook/recreate the landscape...
		HoudiniAssetComponent->StartTaskAssetCookingManual();
	}
	*/

	FHoudiniEngineUtils::UpdateEditorProperties(true);

	if (GEditor)
		GEditor->RedrawAllViewports();
}

void
FHoudiniOutputDetails::OnMaterialInterfaceSelected(
	const FAssetData & AssetData,
	const TWeakObjectPtr<UObject> OutputObject,
	const TWeakObjectPtr<UHoudiniOutput> InOutput,
	int32 MaterialIdx)
{
	TPairInitializer<const TWeakObjectPtr<UObject>&, int32> Pair(OutputObject, MaterialIdx);
	TSharedPtr<SComboButton> AssetComboButton = MaterialInterfaceComboButtons[Pair];
	if (AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject * Object = AssetData.GetAsset();

		UStaticMesh* SM = Cast<UStaticMesh>(OutputObject);
		if (IsValid(SM))
		{
			return OnMaterialInterfaceDropped(Object, SM, InOutput, MaterialIdx);
		}

		ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(OutputObject);
		if (IsValid(Landscape))
		{
			return OnMaterialInterfaceDropped(Object, Landscape, InOutput, MaterialIdx);
		}		
	}
}

void 
FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface(
	const TWeakObjectPtr<UObject> OutputObject,
	const TWeakObjectPtr<UHoudiniOutput> InOutput,
	int32 MaterialIdx) 
{
	if (!IsValidWeakPointer(OutputObject))
		return;

	if (!IsValidWeakPointer(InOutput))
		return;

	if (GEditor)
	{
		TArray<FAssetData> CBSelections;
		GEditor->GetContentBrowserSelections(CBSelections);

		// Get the first selected material object
		UObject* Object = nullptr;
		for (auto & CurAssetData : CBSelections)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
			if (CurAssetData.AssetClassPath != UMaterial::StaticClass()->GetClassPathName() &&
				CurAssetData.AssetClassPath != UMaterialInstance::StaticClass()->GetClassPathName() &&
				CurAssetData.AssetClassPath != UMaterialInstanceConstant::StaticClass()->GetClassPathName())
#else
			if (CurAssetData.AssetClass != UMaterial::StaticClass()->GetFName() &&
				CurAssetData.AssetClass != UMaterialInstance::StaticClass()->GetFName() &&
				CurAssetData.AssetClass != UMaterialInstanceConstant::StaticClass()->GetFName())
#endif
				continue;

			Object = CurAssetData.GetAsset();
			break;
		}

		if (IsValid(Object))
		{
			UStaticMesh* SM = Cast<UStaticMesh>(OutputObject);
			if (IsValid(SM))
			{
				return OnMaterialInterfaceDropped(Object, SM, InOutput, MaterialIdx);
			}

			ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(OutputObject);
			if (IsValid(Landscape))
			{
				return OnMaterialInterfaceDropped(Object, Landscape, InOutput, MaterialIdx);
			}
		}
	}
}

FHoudiniInstancedOutput* GetInstancedOutput(const TWeakObjectPtr<UHoudiniOutput>& InOutput, const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	if (!IsValidWeakPointer(InOutput))
		return nullptr;
	
	return InOutput->GetInstancedOutputs().Find(InIdentifier);
}

void
FHoudiniOutputDetails::CreateInstancerOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	// Do not display instancer UI for one-instance instancers
	bool OnlyOneInstanceInstancers = true;
	for (auto& Iter : InOutput->GetInstancedOutputs())
	{		
		FHoudiniInstancedOutput& CurInstanceOutput = (Iter.Value);
		if (CurInstanceOutput.OriginalTransforms.Num() <= 1)
			continue;
		
		OnlyOneInstanceInstancers = false;
		break;
	}

	// This output only has one-instance instancers (SMC), no need to display the instancer UI.
	if (OnlyOneInstanceInstancers)
		return;

	// Classes allowed for instance variations.
	const TArray<const UClass *> AllowedClasses = 
	{
		UStaticMesh::StaticClass(), USkeletalMesh::StaticClass(),
		AActor::StaticClass(), UBlueprint::StaticClass(),
		UFXSystemAsset::StaticClass(), USoundBase::StaticClass()
	};

	// Classes not allowed for instances variations (useless?)
	TArray<const UClass *> DisallowedClasses =
	{
		UClass::StaticClass(), ULevel::StaticClass(), 
		UMaterial::StaticClass(), UTexture::StaticClass()
	};
	
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// Lambda for adding new variation objects
	auto AddObjectAt = [InOutput](FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex, UObject* InObject)
	{	
		// TODO: undo/redo?
		InOutputToUpdate.VariationObjects.Insert(InObject, AtIndex);
		InOutputToUpdate.VariationTransformOffsets.Insert(FTransform::Identity, AtIndex);
		FHoudiniInstanceTranslator::UpdateVariationAssignements(InOutputToUpdate);

		InOutputToUpdate.MarkChanged(true);

		if (InOutput.IsValid())
			FHoudiniEngineUtils::UpdateEditorProperties(true);
	};

	// Lambda for adding new geometry input objects
	auto RemoveObjectAt = [InOutput](FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex)
	{
		// Also keep one instance object
		if (AtIndex < 0 || AtIndex >= InOutputToUpdate.VariationObjects.Num())
			return;

		if (InOutputToUpdate.VariationObjects.Num() == 1)
			return;

		// TODO: undo/redo?
		InOutputToUpdate.VariationObjects.RemoveAt(AtIndex);
		InOutputToUpdate.VariationTransformOffsets.RemoveAt( AtIndex);
		FHoudiniInstanceTranslator::UpdateVariationAssignements(InOutputToUpdate);

		InOutputToUpdate.MarkChanged(true);

		if (InOutput.IsValid())
			FHoudiniEngineUtils::UpdateEditorProperties(true);
	};

	// Lambda for updating a variation
	auto SetObjectAt = [InOutput](FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex, UObject* InObject)
	{
		if (!InOutputToUpdate.VariationObjects.IsValidIndex(AtIndex))
			return;

		InOutputToUpdate.VariationObjects[AtIndex] = InObject;

		InOutputToUpdate.MarkChanged(true);

		if (InOutput.IsValid())
			FHoudiniEngineUtils::UpdateEditorProperties(true);
	};

	// Lambda for changing the transform offset values
	auto ChangeTransformOffsetAt = [InOutput](
		FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex, 
		const float& Value,  const int32& PosRotScaleIndex, const int32& XYZIndex)
	{
		bool bChanged = InOutputToUpdate.SetTransformOffsetAt(Value, AtIndex, PosRotScaleIndex, XYZIndex);
		if (!bChanged)
			return;

		InOutputToUpdate.MarkChanged(true);

		if (GEditor)
			GEditor->RedrawAllViewports();

		if (InOutput.IsValid())
			FHoudiniEngineUtils::UpdateEditorProperties(true);
	};

	// Get this output's OutputObject
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	// Get Runtime settings to get the default bake Folder
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	bool bEnableDeprecatedInstanceVariations = IsValid(HoudiniRuntimeSettings) ? HoudiniRuntimeSettings->bEnableDeprecatedInstanceVariations : false;

	// Iterate on all of the output's HGPO
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		// Not an instancer, skip
		if (CurHGPO.Type != EHoudiniPartType::Instancer)
			continue;

		// Get the label for that instancer
		FString InstancerLabel = InOutput->GetName() + TEXT(" ") + UHoudiniOutput::OutputTypeToString(InOutput->GetType());
		if (CurHGPO.bHasCustomPartName)
			InstancerLabel = CurHGPO.PartName;

		TSharedRef<SVerticalBox> InstancerVerticalBox = SNew(SVerticalBox);
		TSharedPtr<SHorizontalBox> InstancerHorizontalBox = nullptr;

		// Create a new Group for that instancer
		IDetailGroup& InstancerGroup = HouOutputCategory.AddGroup(FName(*InstancerLabel), FText::FromString(InstancerLabel));

		// Now iterate and display the instance outputs that matches this HGPO
		for (auto& Iter : InOutput->GetInstancedOutputs())
		{
			FHoudiniOutputObjectIdentifier& CurOutputObjectIdentifier = Iter.Key;
			if (!CurOutputObjectIdentifier.Matches(CurHGPO))
				continue;

			FHoudiniInstancedOutput& CurInstanceOutput = (Iter.Value);
			
			// Dont display instancer UI for one-instance instancers (SMC)
			if (CurInstanceOutput.OriginalTransforms.Num() <= 1)
				continue;

			if (!bEnableDeprecatedInstanceVariations)
			{
				UObject* InstancedObject = CurInstanceOutput.OriginalObject.LoadSynchronous();
				if (!IsValid(InstancedObject))
				{
					HOUDINI_LOG_WARNING(TEXT("Null Object found for instance "));
					continue;
				}

				// Create thumbnail for this object.
				TSharedPtr<FAssetThumbnail> Thumbnail =
					MakeShareable(new FAssetThumbnail(InstancedObject, 64, 64, AssetThumbnailPool));
				TSharedRef<SVerticalBox> PickerVerticalBox = SNew(SVerticalBox);
				TSharedPtr<SHorizontalBox> PickerHorizontalBox = nullptr;
				TSharedPtr<SBorder> ThumbnailBorder;

				// For the variation name, reuse the instancer label and append the variation index if we have more than one variation
				FString InstanceOutputLabel = FString::Printf(TEXT("%d Instances"), CurInstanceOutput.OriginalInstanceIndices.Num());

				IDetailGroup* DetailGroup = &InstancerGroup;

				// See if we can find the corresponding component to get its type
				FString InstancerType = TEXT("(Instancer)");
				const FHoudiniOutputObject* OutputObject = OutputObjects.Find(CurOutputObjectIdentifier);
				if (OutputObject && OutputObject->OutputComponents.Num() > 0)
					InstancerType = FHoudiniInstanceTranslator::GetInstancerTypeFromComponent(OutputObject->OutputComponents[0]);

				DetailGroup->AddWidgetRow()
					.NameContent()
					[
						//SNew(SSpacer)
						SNew(STextBlock)
						.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
						.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
						.Text(FText::FromString(InstanceOutputLabel))

					]
					.ValueContent()
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						PickerVerticalBox
					];

				PickerVerticalBox->AddSlot()
					.Padding(0, 2)
					.AutoHeight()
					[
						SAssignNew(PickerHorizontalBox, SHorizontalBox)
					];

				PickerHorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
					[
						SAssignNew(ThumbnailBorder, SBorder)
						.Padding(5.0f)
						.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, TWeakObjectPtr<UObject>(InstancedObject))
						[
							SNew(SBox)
								.WidthOverride(64)
								.HeightOverride(64)
								.ToolTipText(FText::FromString(InstancedObject->GetPathName()))
								[
									Thumbnail->MakeThumbnailWidget()
								]
						]
					];

				// Create tooltip.
				FFormatNamedArguments Args;
				Args.Add(TEXT("Asset"), FText::FromString(InstancedObject->GetName()));
				FText Tooltip =
					FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

				PickerHorizontalBox->AddSlot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						PropertyCustomizationHelpers::MakeBrowseButton(
							FSimpleDelegate::CreateLambda([InOutput, CurOutputObjectIdentifier]()
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;

								UObject* InputObject = InstanceOutput->OriginalObject.LoadSynchronous();

								if (GEditor && InputObject)
								{
									TArray<UObject*> Objects;
									Objects.Add(InputObject);
									GEditor->SyncBrowserToObjects(Objects);
								}
							}),
							TAttribute< FText >(Tooltip))
					];

			}
			else
			{
				FString DeprecatedWarning = TEXT("WARNING: INSTANCE VARIATIONS ARE DEPCRECATED AND WILL BE REMOVED IN THE NEXT VERSION");

				InstancerGroup.AddWidgetRow()
					.NameContent()
					[
						//SNew(SSpacer)
						SNew(STextBlock)
						.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
						.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
						.Text(FText::FromString(DeprecatedWarning))
						.ColorAndOpacity(FLinearColor::Red) 
						.AutoWrapText(true)
					];

				for( int32 VariationIdx = 0; VariationIdx < CurInstanceOutput.VariationObjects.Num(); VariationIdx++ )
				{
					UObject * InstancedObject = CurInstanceOutput.VariationObjects[VariationIdx].LoadSynchronous();
					if ( !IsValid(InstancedObject) )
					{
						HOUDINI_LOG_WARNING( TEXT("Null Object found for instance variation %d"), VariationIdx );
						continue;
					}

					// Create thumbnail for this object.
					TSharedPtr<FAssetThumbnail> VariationThumbnail =
						MakeShareable(new FAssetThumbnail(InstancedObject, 64, 64, AssetThumbnailPool));
					TSharedRef<SVerticalBox> PickerVerticalBox = SNew(SVerticalBox);
					TSharedPtr<SHorizontalBox> PickerHorizontalBox = nullptr;
					TSharedPtr<SBorder> VariationThumbnailBorder;

					// For the variation name, reuse the instancer label and append the variation index if we have more than one variation
					FString InstanceOutputLabel = InstancerLabel;
					if(CurInstanceOutput.VariationObjects.Num() > 1)
						InstanceOutputLabel += TEXT(" [") + FString::FromInt(VariationIdx) + TEXT("]");

					IDetailGroup* DetailGroup = &InstancerGroup;
					if (CurInstanceOutput.VariationObjects.Num() > 1)
					{
						// If we have more than one variation, add a new group for each variation
						DetailGroup = &InstancerGroup.AddGroup(FName(*InstanceOutputLabel), FText::FromString(InstanceOutputLabel), true);
					}

					// See if we can find the corresponding component to get its type
					FString InstancerType = TEXT("(Instancer)"); 
					FHoudiniOutputObjectIdentifier CurVariationIdentifier = CurOutputObjectIdentifier;
					CurVariationIdentifier.SplitIdentifier += TEXT("_") + FString::FromInt(VariationIdx);
					const FHoudiniOutputObject* VariationOutputObject = OutputObjects.Find(CurVariationIdentifier);
					if(VariationOutputObject && VariationOutputObject->OutputComponents.Num() > 0)
						InstancerType = FHoudiniInstanceTranslator::GetInstancerTypeFromComponent(VariationOutputObject->OutputComponents[0]);

					FString VariationLabel = TEXT(" [") + CurVariationIdentifier.SplitIdentifier + TEXT("] ") + InstancerType;
					DetailGroup->AddWidgetRow()
					.NameContent()
					[
						//SNew(SSpacer)
						SNew(STextBlock)
						.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
						.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
						.Text(FText::FromString(VariationLabel))
						//.Size(FVector2D(250, 64))
					]
					.ValueContent()
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						PickerVerticalBox
					];

					// Add an asset drop target
					PickerVerticalBox->AddSlot()
					.Padding(0, 2)
					.AutoHeight()
					[
						SNew(SAssetDropTarget)
						.OnAreAssetsAcceptableForDrop(SAssetDropTarget::FAreAssetsAcceptableForDrop::CreateLambda(
							[DisallowedClasses](TArrayView<FAssetData> InAssets)
							{
								UObject* Obj = InAssets[0].GetAsset();
								for (auto Klass : DisallowedClasses)
								{
									if (Obj && Obj->IsA(Klass))
										return false;
								}
								return true;
							})
						)
						.OnAssetsDropped_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, SetObjectAt](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
						{
							FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
							if (!InstanceOutput)
								return;
							
							return SetObjectAt(*InstanceOutput, VariationIdx, InAssets[0].GetAsset());
						})
						[
							SAssignNew(PickerHorizontalBox, SHorizontalBox)
						]
					];

					PickerHorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
					[
						SAssignNew(VariationThumbnailBorder, SBorder)
						.Padding( 5.0f )
						.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, TWeakObjectPtr<UObject>(InstancedObject))
						[
							SNew(SBox)
							.WidthOverride(64)
							.HeightOverride(64)
							.ToolTipText(FText::FromString(InstancedObject->GetPathName()))
							[
								VariationThumbnail->MakeThumbnailWidget()
							]
						]
					];

					TWeakPtr<SBorder> WeakVariationThumbnailBorder(VariationThumbnailBorder);
					VariationThumbnailBorder->SetBorderImage(TAttribute< const FSlateBrush *>::Create(
						TAttribute<const FSlateBrush *>::FGetter::CreateLambda([WeakVariationThumbnailBorder]()
						{
							TSharedPtr<SBorder> ThumbnailBorder = WeakVariationThumbnailBorder.Pin();
							if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
								return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailLight");
							else
								return _GetEditorStyle().GetBrush("PropertyEditor.AssetThumbnailShadow");
						}
					)));

						PickerHorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 28.0f, 0.0f, 28.0f)
						[
							PropertyCustomizationHelpers::MakeAddButton(
								FSimpleDelegate::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx, AddObjectAt]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return;
									
									UObject* ObjToAdd = InstanceOutput->VariationObjects.IsValidIndex(VariationIdx) ?
										InstanceOutput->VariationObjects[VariationIdx].LoadSynchronous()
										: nullptr;

									return AddObjectAt(*InstanceOutput, VariationIdx, ObjToAdd);
								}),
								LOCTEXT("AddAnotherInstanceToolTip", "Add Another Instance"))
						];

						PickerHorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 28.0f, 4.0f, 28.0f )
						[
							PropertyCustomizationHelpers::MakeRemoveButton(
								FSimpleDelegate::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx, RemoveObjectAt]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return;

									return RemoveObjectAt(*InstanceOutput, VariationIdx);
								}),
								LOCTEXT("RemoveLastInstanceToolTip", "Remove Last Instance"))
						];

					TSharedPtr<SComboButton> AssetComboButton;
					TSharedPtr<SHorizontalBox> ButtonBox;
						PickerHorizontalBox->AddSlot()
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
									//.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
									.ButtonStyle(_GetEditorStyle(), "PropertyEditor.AssetComboStyle")
									.ForegroundColor(_GetEditorStyle().GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
								/* TODO: Update UI
								.OnMenuOpenChanged( FOnIsOpenChanged::CreateUObject(
									&InParam, &UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton,
									CurInstanceOutput, InstOutIdx, VariationIdx ) )
									*/
									.ContentPadding(2.0f)
									.ButtonContent()
									[
										SNew(STextBlock)
										.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
										.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
										.Text(FText::FromString(InstancedObject->GetName()))
									]
								]
							]
						];

						// Create asset picker for this combo button.
						{
							TWeakPtr<SComboButton> WeakAssetComboButton(AssetComboButton);
							TArray<UFactory *> NewAssetFactories;
							TSharedRef<SWidget> PropertyMenuAssetPicker = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
								FAssetData(InstancedObject),
								true,
								AllowedClasses,
								DisallowedClasses,
								NewAssetFactories,
								FOnShouldFilterAsset(),
								FOnAssetSelected::CreateLambda(
									[InOutput, CurOutputObjectIdentifier, VariationIdx, SetObjectAt, WeakAssetComboButton](const FAssetData& AssetData)
									{
										FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
										if (!InstanceOutput)
											return;

										TSharedPtr<SComboButton> AssetComboButtonPtr = WeakAssetComboButton.Pin();
										if (AssetComboButtonPtr.IsValid())
										{
											AssetComboButtonPtr->SetIsOpen(false);
											UObject * Object = AssetData.GetAsset();
											SetObjectAt(*InstanceOutput, VariationIdx, Object);
										}
									}
								),
								// Nothing to do on close
								FSimpleDelegate::CreateLambda([](){})
							);

							AssetComboButton->SetMenuContent(PropertyMenuAssetPicker);
						}

						// Create tooltip.
						FFormatNamedArguments Args;
						Args.Add(TEXT("Asset"), FText::FromString(InstancedObject->GetName()));
						FText StaticMeshTooltip =
							FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

						ButtonBox->AddSlot()
							.AutoWidth()
							.Padding(2.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								PropertyCustomizationHelpers::MakeBrowseButton(
									FSimpleDelegate::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
										{
											FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
											if (!InstanceOutput)
												return;

											UObject* InputObject = InstanceOutput->VariationObjects.IsValidIndex(VariationIdx) ?
												InstanceOutput->VariationObjects[VariationIdx].LoadSynchronous()
												: nullptr;

											if (GEditor && InputObject)
											{
												TArray<UObject*> Objects;
												Objects.Add(InputObject);
												GEditor->SyncBrowserToObjects(Objects);
											}
										}),
									TAttribute< FText >(StaticMeshTooltip))
							];

						ButtonBox->AddSlot()
							.AutoWidth()
							.Padding(2.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ToolTipText(LOCTEXT("ResetToBase", "Reset to default static mesh"))
							.ButtonStyle(_GetEditorStyle(), "NoBorder")
							.ContentPadding(0)
							.Visibility(EVisibility::Visible)
							.OnClicked_Lambda([SetObjectAt, InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return FReply::Handled();

									SetObjectAt(*InstanceOutput, VariationIdx, InstanceOutput->OriginalObject.LoadSynchronous());
									return FReply::Handled();
								})
							[
								SNew(SImage)
								.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
							]
						];


					// Get Visibility of reset buttons
					bool bResetButtonVisiblePosition = false;
					bool bResetButtonVisibleRotation = false;
					bool bResetButtonVisibleScale = false;

					FTransform CurTransform = CurInstanceOutput.VariationTransformOffsets[VariationIdx];

					if (CurTransform.GetLocation() != FVector3d::ZeroVector)
						bResetButtonVisiblePosition = true;

					FRotator Rotator = CurTransform.Rotator();
					if (Rotator.Roll != 0 || Rotator.Pitch != 0 || Rotator.Yaw != 0)
						bResetButtonVisibleRotation = true;

					if (CurTransform.GetScale3D() != FVector3d::OneVector)
						bResetButtonVisibleScale = true;
					
					auto ChangeTransformOffsetUniformlyAt = [ChangeTransformOffsetAt, VariationIdx, InOutput, CurOutputObjectIdentifier](const float& Val, const int32& PosRotScaleIndex)
					{
						FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
						if (!InstanceOutput)
							return;

						ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, PosRotScaleIndex, 0);
						ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, PosRotScaleIndex, 1);
						ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, PosRotScaleIndex, 2);
					};

					TSharedRef<SVerticalBox> OffsetVerticalBox = SNew(SVerticalBox);
					FText LabelPositionText = LOCTEXT("HoudiniPositionOffset", "Position Offset");
					DetailGroup->AddWidgetRow()
					.NameContent()
					[
						SNew(STextBlock)
						.Text(LabelPositionText)
						.ToolTipText(LabelPositionText)
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
					.ValueContent()
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						[
							SNew(SVectorInputBox)
							.bColorAxisLabels(true)
							.AllowSpin(true)
							.X(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return 0.0f;
									return InstanceOutput->GetTransformOffsetAt(VariationIdx, 0, 0);
								}
							)))
							.Y(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return 0.0f;
									return InstanceOutput->GetTransformOffsetAt(VariationIdx, 0, 1);
								}
							)))
							.Z(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return 0.0f;
									return InstanceOutput->GetTransformOffsetAt(VariationIdx, 0, 2);
								}
							)))
							.OnXCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 0, 0);
							})	
							.OnYCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 0, 1);
							})
							.OnZCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 0, 2);
							})
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							// Lock Button (not visible)
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
							[
								SNew(SButton)
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ClickMethod(EButtonClickMethod::MouseDown)
								.Visibility(EVisibility::Hidden)
								[
									SNew(SImage)
									.Image(_GetEditorStyle().GetBrush("GenericLock"))
								]
							]
							// Reset Button
							+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
							[
								SNew(SButton)
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ClickMethod(EButtonClickMethod::MouseDown)
								.ToolTipText(LOCTEXT("InstancerOutputResetButtonToolTip", "Reset To Default"))
								.Visibility(bResetButtonVisiblePosition ? EVisibility::Visible : EVisibility::Hidden)
								[
									SNew(SImage)
									.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
								]
								.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, CurInstanceOutput, InOutput]()
								{
									ChangeTransformOffsetUniformlyAt(0.0f, 0);
									FHoudiniEngineUtils::UpdateEditorProperties(true);
									return FReply::Handled();
								})
							]
						]
					];

					FText LabelRotationText = LOCTEXT("HoudiniRotationOffset", "Rotation Offset");
					DetailGroup->AddWidgetRow()
					.NameContent()
					[
						SNew(STextBlock)
						.Text(LabelRotationText)
						.ToolTipText(LabelRotationText)
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
					.ValueContent()
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						[
							SNew(SRotatorInputBox)
							.AllowSpin(true)
							.bColorAxisLabels(true)                    
							.Roll(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return 0.0f;
									return InstanceOutput->GetTransformOffsetAt(VariationIdx, 1, 0);
								}
							)))
							.Pitch(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return 0.0f;
									return InstanceOutput->GetTransformOffsetAt(VariationIdx, 1, 1);
								}
							)))
							.Yaw(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return 0.0f;
									return InstanceOutput->GetTransformOffsetAt(VariationIdx, 1, 2);
								}
							)))
							.OnRollCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 1, 0);
							})	
							.OnPitchCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 1, 1);
							})
							.OnYawCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 1, 2);
							})
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							// Lock Button (not visible)
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
							[
								SNew(SButton)
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ClickMethod(EButtonClickMethod::MouseDown)
								.Visibility(EVisibility::Hidden)
								[
									SNew(SImage)
									.Image(_GetEditorStyle().GetBrush("GenericLock"))
								]
							]
							// Reset Button
							+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
							[
								SNew(SButton)
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ClickMethod(EButtonClickMethod::MouseDown)
								.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
								.Visibility(bResetButtonVisibleRotation ? EVisibility::Visible : EVisibility::Hidden)
								[
									SNew(SImage)
									.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
								]
								.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, InOutput]()
								{
									ChangeTransformOffsetUniformlyAt(0.0f, 1);
									FHoudiniEngineUtils::UpdateEditorProperties(true);
									return FReply::Handled();
								})
							]
						]
					];

					FText LabelScaleText = LOCTEXT("HoudiniScaleOffset", "Scale Offset");
					DetailGroup->AddWidgetRow()
					.NameContent()
					[
						SNew(STextBlock)
						.Text(LabelScaleText)
						.ToolTipText(LabelScaleText)
						.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
					.ValueContent()
					.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
						[
							SNew(SVectorInputBox)
							.bColorAxisLabels(true)
							.X(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
									{ 
										FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
										if (!InstanceOutput)
											return 1.0f;
										return InstanceOutput->GetTransformOffsetAt(VariationIdx, 2, 0); 
									}
							)))
							.Y(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
									{ 
										FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
										if (!InstanceOutput)
											return 1.0f;
										return InstanceOutput->GetTransformOffsetAt(VariationIdx, 2, 1); 
									}
							)))
							.Z(TAttribute<TOptional<float>>::Create(
								TAttribute<TOptional<float>>::FGetter::CreateLambda([InOutput, CurOutputObjectIdentifier, VariationIdx]()
									{ 
										FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
										if (!InstanceOutput)
											return 1.0f;
										return InstanceOutput->GetTransformOffsetAt(VariationIdx, 2, 2); 
									}
							)))
							.OnXCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt, ChangeTransformOffsetUniformlyAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								if (InstanceOutput->IsUnformScaleLocked())
									ChangeTransformOffsetUniformlyAt(Val, 2);
								else
									ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 2, 0); 
							})	
							.OnYCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt, ChangeTransformOffsetUniformlyAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								if (InstanceOutput->IsUnformScaleLocked())
									ChangeTransformOffsetUniformlyAt(Val, 2);
								else
									ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 2, 1); 
							})
							.OnZCommitted_Lambda([InOutput, CurOutputObjectIdentifier, VariationIdx, ChangeTransformOffsetAt, ChangeTransformOffsetUniformlyAt](float Val, ETextCommit::Type TextCommitType)
							{
								FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
								if (!InstanceOutput)
									return;
								if (InstanceOutput->IsUnformScaleLocked())
									ChangeTransformOffsetUniformlyAt(Val, 2);
								else
									ChangeTransformOffsetAt(*InstanceOutput, VariationIdx, Val, 2, 2);
							})
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							// Lock Button
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
							[
								SNew(SButton)
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ClickMethod(EButtonClickMethod::MouseDown)
								.ToolTipText(LOCTEXT("InstancerOutputLockButtonToolTip", "When locked, scales uniformly based on the current xyz scale values so the output object maintains its shape in each direction when scaled"))
								.Visibility(EVisibility::Visible)
								[
									SNew(SImage)
									.Image(CurInstanceOutput.IsUnformScaleLocked() ? _GetEditorStyle().GetBrush("GenericLock") : _GetEditorStyle().GetBrush("GenericUnlock"))
								]
								.OnClicked_Lambda([InOutput, CurOutputObjectIdentifier]() 
								{
									FHoudiniInstancedOutput* InstanceOutput = GetInstancedOutput(InOutput, CurOutputObjectIdentifier);
									if (!InstanceOutput)
										return FReply::Handled();
									
									InstanceOutput->SwitchUniformScaleLock();
									
									FHoudiniEngineUtils::UpdateEditorProperties(true);
									return FReply::Handled();
								})
							]
							// Reset Button
							+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
							[
								SNew(SButton)
								.ButtonStyle(_GetEditorStyle(), "NoBorder")
								.ClickMethod(EButtonClickMethod::MouseDown)
								.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
								.Visibility(bResetButtonVisibleScale ? EVisibility::Visible : EVisibility::Hidden)
								[
									SNew(SImage)
									.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
								]
								.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, InOutput]()
								{
									ChangeTransformOffsetUniformlyAt(1.0f, 2);
									FHoudiniEngineUtils::UpdateEditorProperties(true);
									return FReply::Handled();
								})
							]
						]
						/*
						// TODO: Add support for this back
						+ SHorizontalBox::Slot().AutoWidth()
						[
							// Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
							SNew(SCheckBox)
							.Style(_GetEditorStyle(), "TransparentCheckBox")
							.ToolTipText(LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled"))
							*//*
							.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState NewState)
							{
								if ( MyParam.IsValid() && InputFieldPtr.IsValid() )
									MyParam->CheckStateChanged( NewState == ECheckBoxState::Checked, InputFieldPtr.Get(), VariationIdx );
							}))
							.IsChecked( TAttribute< ECheckBoxState >::Create(
								TAttribute<ECheckBoxState>::FGetter::CreateLambda( [=]() 
								{
									if (InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly(VariationIdx))
										return ECheckBoxState::Checked;
									return ECheckBoxState::Unchecked;
								}
							)))
							*//*
							[
								SNew(SImage)
								*//*.Image(TAttribute<const FSlateBrush*>::Create(
									TAttribute<const FSlateBrush*>::FGetter::CreateLambda( [=]() 
									{
										if ( InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly( VariationIdx ) )
										{
											return FEditorStyle::GetBrush( TEXT( "GenericLock" ) );
										}
										return FEditorStyle::GetBrush( TEXT( "GenericUnlock" ) );
									}
								)))
								*//*
								.ColorAndOpacity( FSlateColor::UseForeground() )
							]
						]
						*/
					];
					}
				}
			}
		}
	}

/*
void
FHoudiniOutputDetails::OnMaterialInterfaceSelected(
	const FAssetData & AssetData, 
	ALandscapeProxy* Landscape,
	UHoudiniOutput * InOutput,
	int32 MaterialIdx)
{
	TPairInitializer< ALandscapeProxy *, int32 > Pair(Landscape, MaterialIdx);
	TSharedPtr< SComboButton > AssetComboButton = LandscapeMaterialInterfaceComboButtons[Pair];
	if (AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject * Object = AssetData.GetAsset();
		OnMaterialInterfaceDropped(Object, Landscape, InOutput, MaterialIdx);
	}
}
*/

void
FHoudiniOutputDetails::CreateDefaultOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput)
{
	if (!IsValidWeakPointer(InOutput))
		return;

	// Get thumbnail pool for this builder.
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouOutputCategory.GetParentLayout().GetThumbnailPool();

	// TODO
	// This is just a temporary placeholder displaying name/output type
	{
		FString OutputNameStr = InOutput->GetName();
		FText OutputTooltip = GetOutputTooltip(InOutput);

		// Create a new detail row
		// Name 
		FText OutputNameTxt = GetOutputDebugName(InOutput);
		FString OutputRowString = TEXT("Output ") + OutputNameStr;
		FDetailWidgetRow & Row = HouOutputCategory.AddCustomRow(FText::FromString(OutputRowString));
		Row.NameWidget.Widget =
			SNew(STextBlock)
			.Text(OutputNameTxt)
			.ToolTipText(OutputTooltip)
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		// Value
		FText OutputTypeTxt = GetOutputDebugDescription(InOutput);
		Row.ValueWidget.Widget =
			SNew(STextBlock)
			.Text(OutputTypeTxt)
			.ToolTipText(OutputTooltip)
			.Font(_GetEditorStyle().GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	}
}

void
FHoudiniOutputDetails::OnBakeOutputObject(
	const FString& InBakeName,
	UObject * BakedOutputObject, 
	const FHoudiniOutputObjectIdentifier & OutputIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const FHoudiniGeoPartObject & HGPO,
	const UObject* OutputOwner,
	UHoudiniOutput* InOutput,
	const FString & BakeFolder,
	const FHoudiniBakeSettings& BakeSettings,
	const FString & TempCookFolder,
	const EHoudiniLandscapeOutputBakeType & LandscapeBakeType,
	const TArray<UHoudiniOutput*>& InAllOutputs)
{
	if (!IsValid(BakedOutputObject) || !IsValid(InOutput))
		return;

	// Fill in the package params
	FHoudiniPackageParams PackageParams;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniAttributeResolver Resolver;
	// Determine the relevant WorldContext based on the output owner
	UWorld* WorldContext = OutputOwner ? OutputOwner->GetWorld() : GWorld;
	UHoudiniAssetComponent* const HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(OutputOwner);
	check(IsValid(HAC));

	// Check if we have previously baked this object on this HAC
	bool bHasPreviousBakeData = false;
	FHoudiniBakedOutputObject BakedObjectEntry;
	const int32 OutputIndex = InAllOutputs.IndexOfByKey(InOutput);
	if (OutputIndex >= 0 && HAC->GetBakedOutputs().IsValidIndex(OutputIndex))
	{
		FHoudiniBakedOutputObject const* const PrevBakedOutputObject = HAC->GetBakedOutputs()[OutputIndex].BakedOutputObjects.Find(OutputIdentifier);
		if (PrevBakedOutputObject)
		{
			bHasPreviousBakeData = true;
			BakedObjectEntry = *PrevBakedOutputObject;
		}
	}
	
	const FString HoudiniAssetName = HAC->GetHoudiniAssetName();
	const FString HoudiniAssetActorName = IsValid(HAC->GetOwner()) ? HAC->GetOwner()->GetActorNameOrLabel() : TEXT("");
	const bool bAutomaticallySetAttemptToLoadMissingPackages = true;
	const bool bSkipObjectNameResolutionAndUseDefault = !InBakeName.IsEmpty();  // If InBakeName is set use it as is for the object name
	const bool bSkipBakeFolderResolutionAndUseDefault = false;

	FString DefaultObjectName;
	if (InBakeName.IsEmpty())
	{
		// First get names from previous bake entry
		if (bHasPreviousBakeData)
		{
			UObject const* const PrevObject = BakedObjectEntry.GetBakedObjectIfValid();
			if (PrevObject)
			{
				DefaultObjectName = PrevObject->GetName();
			}
			else
			{
				AActor const* const PrevActor = BakedObjectEntry.GetActorIfValid();
				if (PrevActor)
					DefaultObjectName = PrevActor->GetName();
			}
		}

		// fallback to name from BakedOutputObject
		if (DefaultObjectName.IsEmpty())
			DefaultObjectName = BakedOutputObject->GetName();
	}
	else
	{
		DefaultObjectName = InBakeName;
	}
	
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		WorldContext, HAC, OutputIdentifier, InOutputObject, bHasPreviousBakeData, BakedOutputObject->GetName(),
		PackageParams, Resolver, BakeFolder, EPackageReplaceMode::ReplaceExistingAssets,
		HoudiniAssetName, HoudiniAssetActorName,
		bAutomaticallySetAttemptToLoadMissingPackages, bSkipObjectNameResolutionAndUseDefault,
		bSkipBakeFolderResolutionAndUseDefault);

	const EHoudiniOutputType Type = InOutput->GetType();

	FHoudiniBakedObjectData BakeOutputs;

	switch (Type) 
	{
		case EHoudiniOutputType::Mesh:
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(BakedOutputObject);
			if (StaticMesh)
			{
				FDirectoryPath TempCookFolderPath;
				TempCookFolderPath.Path = TempCookFolder;
				TMap<UStaticMesh*, UStaticMesh*> AlreadyBakedStaticMeshMap;
				TMap<UMaterialInterface *, UMaterialInterface *> AlreadyBakedMaterialsMap;
				UStaticMesh* DuplicatedMesh = FHoudiniEngineBakeUtils::BakeStaticMesh(
					StaticMesh, PackageParams, InAllOutputs, TempCookFolderPath, AlreadyBakedStaticMeshMap, AlreadyBakedMaterialsMap);

				BakedObjectEntry.Actor.Empty();
				BakedObjectEntry.BakedComponent.Empty();
				BakedObjectEntry.BakedObject = FSoftObjectPath(DuplicatedMesh).ToString();
			}
		}
		break;
		case EHoudiniOutputType::Curve:
		{
			USplineComponent* SplineComponent = Cast<USplineComponent>(BakedOutputObject);
			if (SplineComponent)
			{
				AActor* BakedActor;
				USplineComponent* BakedSplineComponent;
				FHoudiniEngineBakeUtils::BakeCurve(
					HAC, SplineComponent, GWorld->GetCurrentLevel(), PackageParams, BakeSettings, FName(PackageParams.ObjectName), BakedActor, BakedSplineComponent, BakeOutputs);

				BakedObjectEntry.Actor = FSoftObjectPath(BakedActor).ToString();
				BakedObjectEntry.BakedComponent = FSoftObjectPath(BakedSplineComponent).ToString();
				BakedObjectEntry.BakedObject.Empty();
			}
		}
		break;
		case EHoudiniOutputType::Landscape:
		{
			ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(BakedOutputObject);
			if (Landscape)
			{
				FHoudiniEngineBakeUtils::BakeHeightfield(Landscape, PackageParams, LandscapeBakeType, BakeOutputs);
				BakedObjectEntry.Actor.Empty();
				BakedObjectEntry.BakedComponent.Empty();
				BakedObjectEntry.BakedObject.Empty();
			}
		}
		break;
	}

	if (OutputIndex >= 0)
	{
		TArray<FHoudiniBakedOutput>& BakedOutputs = HAC->GetBakedOutputs();
		if (!BakedOutputs.IsValidIndex(OutputIndex))
		{
			BakedOutputs.SetNum(OutputIndex + 1);
		}
		BakedOutputs[OutputIndex].BakedOutputObjects.Emplace(OutputIdentifier, BakedObjectEntry);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeOutputs.BakeStats.NumPackagesCreated, BakeOutputs.BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

}

FReply
FHoudiniOutputDetails::OnRefineClicked(const TWeakObjectPtr<UObject> ObjectToRefine, const TWeakObjectPtr<UHoudiniOutput> InOutput)
{	
	// TODO: Actually refine only the selected ProxyMesh
	// For now, refine all the selection
	FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(true, true);
	
	return FReply::Handled();
}

void
FHoudiniOutputDetails::OnBakeNameCommitted(
	const FText& Val, ETextCommit::Type TextCommitType,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput, const FHoudiniOutputObjectIdentifier& InIdentifier) 
{
	if (!IsValidWeakPointer(InOutput))
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);

	if (!FoundOutputObject)
		return;

	FoundOutputObject->BakeName = Val.ToString();
}

void
FHoudiniOutputDetails::OnRevertBakeNameToDefault(const TWeakObjectPtr<UHoudiniOutput>& InOutput, const FHoudiniOutputObjectIdentifier & InIdentifier) 
{
	if (!IsValidWeakPointer(InOutput))
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);

	if (!FoundOutputObject)
		return;

	FoundOutputObject->BakeName = FString();
}


void
FHoudiniOutputDetails::CreateSkeletalMeshAndMaterialWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	const TWeakObjectPtr<UHoudiniOutput>& InOutput,
	const TWeakObjectPtr<USkeletalMesh>& SkelMesh,
	const TWeakObjectPtr<USkeleton>& Skeleton,
	const TWeakObjectPtr<UPhysicsAsset>& PhysicsAsset,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	if (!IsValidWeakPointer(SkelMesh) && !IsValidWeakPointer(Skeleton) && !IsValidWeakPointer(PhysicsAsset))
		return;


	const TWeakObjectPtr<UHoudiniAssetComponent>& OwningHAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());

	FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	FString BakeName = FoundOutputObject ? FoundOutputObject->BakeName : FString();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// TODO: GetBakingBaseName!
	FString SkelLabel;

	if (Skeleton.IsValid())
		SkelLabel = Skeleton->GetName();

	if (SkelMesh.IsValid())
		SkelLabel = SkelMesh->GetName();

	if (HoudiniGeoPartObject.bHasCustomPartName)
		SkelLabel = HoudiniGeoPartObject.PartName;

	// Create thumbnail for this mesh.
	TSharedPtr<FAssetThumbnail> SkeletonThumbnail = MakeShareable(new FAssetThumbnail(Skeleton.Get(), 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> SkeletonThumbnailBorder;

	TSharedPtr<FAssetThumbnail> PhysicsAssetThumbnail = MakeShareable(new FAssetThumbnail(PhysicsAsset.Get(), 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> PhysicsAssetThumbnailBorder;

	TSharedRef<SVerticalBox> SkeletonVerticalBox = SNew(SVerticalBox);
	TSharedRef<SVerticalBox> PhysicsAssetVerticalBox = SNew(SVerticalBox);

	IDetailGroup& SkeletonGrp = HouOutputCategory.AddGroup(FName(*SkelLabel), FText::FromString(SkelLabel));

	// Add details on the Skeleton.

	if (IsValid(Skeleton.Get()))
	{
		FString SkeletonLabel = TEXT("Skeleton");

		SkeletonGrp.AddWidgetRow()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(SkeletonLabel))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				SkeletonVerticalBox
			];

		SkeletonVerticalBox->AddSlot()
			.Padding(0, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(SkeletonThumbnailBorder, SBorder)
						.Padding(5.0f)
						.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&)Skeleton)
						.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) Skeleton)
						[
							SNew(SBox)
							.WidthOverride(64)
							.HeightOverride(64)
							.ToolTipText(FText::FromString(Skeleton->GetPathName()))
							[
								SkeletonThumbnail->MakeThumbnailWidget()
							]
						]
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 4.0f, 4.0f, 4.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f, 0.0f)
								.VAlign(VAlign_Center)
								[
									PropertyCustomizationHelpers::MakeBrowseButton(
										FSimpleDelegate::CreateSP(
											this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)Skeleton),
										TAttribute<FText>(LOCTEXT("HoudiniSkelMeshBrowseButton", "Browse to this generated Skeleton in the content browser")))
								]
						]
					]
			];

		// Store thumbnail for this mesh.
		OutputObjectThumbnailBorders.Add(Skeleton, SkeletonThumbnailBorder);	
	}

	if (IsValid(PhysicsAsset.Get()))
	{
		FString PhysicsAssetLabel = TEXT("Physics Asset");

		SkeletonGrp.AddWidgetRow()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(PhysicsAssetLabel))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				PhysicsAssetVerticalBox
			];

			PhysicsAssetVerticalBox->AddSlot()
			.Padding(0, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(PhysicsAssetThumbnailBorder, SBorder)
							.Padding(5.0f)
							.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&)PhysicsAsset)
							.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) PhysicsAsset)
							[
								SNew(SBox)
								.WidthOverride(64)
								.HeightOverride(64)
								.ToolTipText(FText::FromString(PhysicsAsset->GetPathName()))
								[
									PhysicsAssetThumbnail->MakeThumbnailWidget()
								]
							]
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 4.0f, 4.0f, 4.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f, 0.0f)
								.VAlign(VAlign_Center)
								[
									PropertyCustomizationHelpers::MakeBrowseButton(
										FSimpleDelegate::CreateSP(
											this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)PhysicsAsset),
										TAttribute<FText>(LOCTEXT("HoudiniSkelMeshBrowseButton", "Browse to this Physics Asset in the content browser")))
								]
						]
					]
			];

		// Store thumbnail for this mesh.
		OutputObjectThumbnailBorders.Add(PhysicsAsset, PhysicsAssetThumbnailBorder);
	}

	// SKEL MESH
	if (!IsValidWeakPointer(SkelMesh))
		return;

	// TODO: GetBakingBaseName!
	FString SkelMeshLabel = SkelMesh->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		SkelMeshLabel = HoudiniGeoPartObject.PartName;

	// Create thumbnail for this mesh.
	TSharedPtr<FAssetThumbnail> SkelMeshThumbnail =
		MakeShareable(new FAssetThumbnail(SkelMesh.Get(), 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> SkelMeshThumbnailBorder;

	TSharedRef<SVerticalBox> SKMVerticalBox = SNew(SVerticalBox);

	/*
	SkeletonGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(BakeName))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OnTextCommitted_Lambda([OutputIdentifier, InOutput](const FText& Val, ETextCommit::Type TextCommitType)
			{
				if (!IsValidWeakPointer(InOutput))
					return;

				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(true);
			})
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
		]
	
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(_GetEditorStyle(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]()
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(true);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(_GetEditorStyle().GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];
	*/

	// Add details on the SkelMesh colliders
	FString SKMLabel = TEXT("Skeletal Mesh");

	/*
	// Indicate that this mesh is instanced
	if (HoudiniGeoPartObject.bIsInstanced)
	{
		MeshLabel += TEXT("\n(instanced)");
	}

	if (HoudiniGeoPartObject.bIsTemplated)
	{
		MeshLabel += TEXT("\n(templated)");
	}
	*/
	SkeletonGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(SKMLabel))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SKMVerticalBox
	];

	SKMVerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.AutoWidth()
		[
			SAssignNew(SkelMeshThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (const TWeakObjectPtr<UObject>&)SkelMesh)
			.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&) SkelMesh)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(SkelMesh->GetPathName()))
				[
					SkelMeshThumbnail->MakeThumbnailWidget()
				]
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				/*
				+ SHorizontalBox::Slot()
				.MaxWidth(120.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("BakeOutputMesh", "Bake Output"))
					.IsEnabled(true)
					.OnClicked_Lambda([BakeName, SkelMesh, OutputIdentifier, InOutput, OwningHAC]()
					{
						if (!SkelMesh.IsValid() || !InOutput.IsValid())
							return FReply::Handled();

						FHoudiniOutputObject* const FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
						if (!FoundOutputObject)
							return FReply::Handled();

						TArray<UHoudiniOutput*> AllOutputs;
						FString TempCookFolder;
						FString BakeFolder;
						if (OwningHAC.IsValid())
						{
							AllOutputs.Reserve(OwningHAC->GetNumOutputs());
							OwningHAC->GetOutputs(AllOutputs);

							TempCookFolder = OwningHAC->TemporaryCookFolder.Path;
							BakeFolder = OwningHAC->BakeFolder.Path;
						}

						FHoudiniGeoPartObject HoudiniGeoPartObject;
						for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects())
						{
							if (!OutputIdentifier.Matches(curHGPO))
								continue;

							HoudiniGeoPartObject = curHGPO;
							break;
						}

						FHoudiniOutputDetails::OnBakeOutputObject(
							BakeName,
							SkelMesh.Get(),
							OutputIdentifier,
							*FoundOutputObject,
							HoudiniGeoPartObject,
							OwningHAC.Get(),
							InOutput.Get(),
							BakeFolder,
							TempCookFolder,
							EHoudiniLandscapeOutputBakeType::InValid,
							AllOutputs);

						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("HoudiniSkelMeshBakeButton", "Bake this output object to a Skeletal Mesh in the Bake folder."))
				]
				*/
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateSP(
							this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)SkelMesh),
						TAttribute<FText>(LOCTEXT("HoudiniSkelMeshBrowseButton", "Browse to this generated Skeletal Mesh in the content browser")))
				]
			]
		]
	];

	// Store thumbnail for this mesh.
	OutputObjectThumbnailBorders.Add(SkelMesh, SkelMeshThumbnailBorder);

	// We need to add material box for each material present in this static mesh.	
	auto& SkelMeshMaterials = SkelMesh->GetMaterials();
	for (int32 MaterialIdx = 0; MaterialIdx < SkelMeshMaterials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = SkelMeshMaterials[MaterialIdx].MaterialInterface;
		TSharedPtr< SBorder > MaterialThumbnailBorder;
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if (IsValid(MaterialInterface)
			&& IsValid(MaterialInterface->GetOuter()))
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}
		else
		{
			MaterialInterface = nullptr;
			MaterialName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
			MaterialPathName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
		}

		// Create thumbnail for this material.
		TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
			MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

		SKMVerticalBox->AddSlot()
		.Padding(0, 2)
		[
			/*SNew(SAssetDropTarget)
			.OnAreAssetsAcceptableForDrop(this, false)
			.OnAssetsDropped(
				this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, SkelMesh, InOutput, MaterialIdx)*/
			SNew(SBox)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.AutoWidth()
		[
			SAssignNew(MaterialThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(
				this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (const TWeakObjectPtr<UObject>&)SkelMesh, MaterialIdx)
			.OnMouseDoubleClick(
				this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (const TWeakObjectPtr<UObject>&)MaterialInterface)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(MaterialPathName))
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this mesh and material index.
		{
			TPairInitializer<const TWeakObjectPtr<USkeletalMesh>&, int32> Pair(SkelMesh, MaterialIdx);
			MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
		}

		// ComboBox and buttons
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Add Combo box
		TSharedPtr<SComboButton> AssetComboButton;
		ComboAndButtonBox->AddSlot()
		.VAlign(VAlign_Center)
		.FillHeight(1.0f)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().VAlign(VAlign_Center).FillHeight(1.0f)
			[
				SAssignNew(AssetComboButton, SComboButton)
				.ButtonStyle(_GetEditorStyle(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(_GetEditorStyle().GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent,
					TWeakObjectPtr<UMaterialInterface>(MaterialInterface), (const TWeakObjectPtr<UObject>&)SkelMesh, InOutput, MaterialIdx)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(_GetEditorStyle(), "PropertyEditor.AssetClass")
					.Font(_GetEditorStyle().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

		// Add buttons
		TSharedPtr<SHorizontalBox> ButtonBox;
		ComboAndButtonBox->AddSlot()
		.FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];

		// Use CB selection arrow button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(const TWeakObjectPtr<UObject>&)SkelMesh, InOutput, MaterialIdx),
					TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")))
		];

		// Browse CB button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (const TWeakObjectPtr<UObject>&)MaterialInterface), TAttribute< FText >(MaterialTooltip))
		];

		// Store combo button for this mesh and index.
		{
			TPairInitializer<const TWeakObjectPtr<USkeletalMesh>&, int32> Pair(SkelMesh, MaterialIdx);
			MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
		}
	}
}


#undef LOCTEXT_NAMESPACE
