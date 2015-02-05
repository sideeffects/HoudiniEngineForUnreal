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


UHoudiniAssetInput::UHoudiniAssetInput(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	InputObject(nullptr),
	InputCurve(nullptr),
	GeometryAssetId(-1),
	CurveAssetId(-1),
	ConnectedAssetId(-1),
	InputIndex(0),
	ChoiceIndex(EHoudiniAssetInputType::GeometryInput),
	bStaticMeshChanged(false),
	bSwitchedToCurve(false),
	bLoadedParameter(false)
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

	// Get name of this input. For the time being we only support geometry inputs.
	HAPI_StringHandle InputStringHandle;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetInputName(InHoudiniAssetComponent->GetAssetId(), InInputIndex, HAPI_INPUT_GEOMETRY, &InputStringHandle))
	{
		return HoudiniAssetInput;
	}

	HoudiniAssetInput = NewObject<UHoudiniAssetInput>(InHoudiniAssetComponent);

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
		FString* ChoiceLabel = new FString(TEXT("Curve Input"));
		StringChoiceLabels.Add(TSharedPtr<FString>(ChoiceLabel));

		if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
		{
			ChoiceStringValue = *ChoiceLabel;
		}
	}
}


void
UHoudiniAssetInput::DestroyHoudiniAssets()
{
	DisconnectInputAsset();

	DestroyGeometryInputAsset();
	DestroyCurveInputAsset();
}


void
UHoudiniAssetInput::DestroyGeometryInputAsset()
{
	// Destroy connected geometry.
	if(-1 != GeometryAssetId)
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(GeometryAssetId);
		GeometryAssetId = -1;
	}
}


void
UHoudiniAssetInput::DestroyCurveInputAsset()
{
	// Destroy connected curve.
	if(-1 != CurveAssetId)
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(CurveAssetId);
		CurveAssetId = -1;
	}

	// We need to destroy spline component.
	if(InputCurve)
	{
		InputCurve->DetachFromParent();
		InputCurve->UnregisterComponent();
		InputCurve->DestroyComponent();

		InputCurve = nullptr;
	}
}


void
UHoudiniAssetInput::DisconnectInputAsset()
{
	HAPI_AssetId HostAssetId = HoudiniAssetComponent->GetAssetId();
	if(FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId) && FHoudiniEngineUtils::IsValidAssetId(HostAssetId))
	{
		FHoudiniEngineUtils::HapiDisconnectAsset(HostAssetId, InputIndex);
		ConnectedAssetId = -1;
	}
}


void
UHoudiniAssetInput::ConnectInputAsset()
{
	// Asset must be disconnected before connecting.
	check(!FHoudiniEngineUtils::IsValidAssetId(ConnectedAssetId));

	HAPI_AssetId HostAssetId = HoudiniAssetComponent->GetAssetId();
	HAPI_AssetId ConnectingAssetId = -1;

	if(EHoudiniAssetInputType::GeometryInput == ChoiceIndex)
	{
		ConnectingAssetId = GeometryAssetId;
	}
	else if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
	{
		ConnectingAssetId = CurveAssetId;
	}

	if(FHoudiniEngineUtils::HapiConnectAsset(ConnectingAssetId, 0, HostAssetId, InputIndex))
	{
		ConnectedAssetId = ConnectingAssetId;
	}
}


bool
UHoudiniAssetInput::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
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

	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SNew(SComboBox<TSharedPtr<FString> >)
		.OptionsSource(&StringChoiceLabels)
		.InitiallySelectedItem(StringChoiceLabels[ChoiceIndex])
		.OnGenerateWidget(SComboBox<TSharedPtr<FString> >::FOnGenerateWidget::CreateUObject(this, &UHoudiniAssetInput::CreateChoiceEntryWidget))
		.OnSelectionChanged(SComboBox<TSharedPtr<FString> >::FOnSelectionChanged::CreateUObject(this, &UHoudiniAssetInput::OnChoiceChange))
		[
			SNew(STextBlock)
			.Text(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateUObject(this, &UHoudiniAssetInput::HandleChoiceContentText)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	if(EHoudiniAssetInputType::GeometryInput == ChoiceIndex)
	{
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
			]
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
	}
	else if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
	{
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(InputCurveParameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			HoudiniAssetParameter->CreateWidget(VerticalBox);
		}
	}

	Row.ValueWidget.Widget = VerticalBox;
	Row.ValueWidget.MinDesiredWidth(FHoudiniAssetComponentDetails::RowValueWidgetDesiredWidth);
}


bool
UHoudiniAssetInput::UploadParameterValue()
{
	HAPI_AssetId HostAssetId = HoudiniAssetComponent->GetAssetId();

	if(EHoudiniAssetInputType::GeometryInput == ChoiceIndex)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(InputObject);
		if(StaticMesh)
		{
			if(bStaticMeshChanged || bLoadedParameter)
			{
				bStaticMeshChanged = false;
				if(!FHoudiniEngineUtils::HapiCreateAndConnectAsset(HostAssetId, InputIndex, StaticMesh, GeometryAssetId))
				{
					bChanged = false;
					return false;
				}

				ConnectedAssetId = GeometryAssetId;
			}
		}
		else
		{
			// Either mesh was reset or null mesh has been assigned.
			DestroyGeometryInputAsset();
		}
	}
	else if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
	{
		// If we have no curve asset, create it.
		if(!FHoudiniEngineUtils::IsValidAssetId(CurveAssetId))
		{
			if(!FHoudiniEngineUtils::HapiCreateCurve(CurveAssetId) || !FHoudiniEngineUtils::IsValidAssetId(CurveAssetId))
			{
				bChanged = false;
				return false;
			}
		}

		// Connect asset if it is not connected.
		if(!IsCurveAssetConnected())
		{
			FHoudiniEngineUtils::HapiConnectAsset(CurveAssetId, 0, HostAssetId, InputIndex);
		}

		if(bLoadedParameter)
		{
			HAPI_AssetInfo CurveAssetInfo;
			FHoudiniApi::GetAssetInfo(CurveAssetId, &CurveAssetInfo);

			// If we just loaded our curve, we need to set parameters.
			for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(InputCurveParameters); IterParams; ++IterParams)
			{
				UHoudiniAssetParameter* Parameter = IterParams.Value();

				// We need to update node id for loaded parameters.
				Parameter->SetNodeId(CurveAssetInfo.nodeId);

				// Upload parameter value.
				Parameter->UploadParameterValue();
			}
		}

		// Also upload points.
		HAPI_NodeId NodeId = -1;
		if(FHoudiniEngineUtils::HapiGetNodeId(CurveAssetId, 0, 0, NodeId))
		{
			const TArray<FVector>& CurvePoints = InputCurve->GetCurvePoints();

			FString PositionString = TEXT("");
			FHoudiniEngineUtils::CreatePositionsString(CurvePoints, PositionString);

			// Get param id.
			HAPI_ParmId ParmId = -1;
			if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmIdFromName(NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, &ParmId))
			{
				std::string ConvertedString = TCHAR_TO_UTF8(*PositionString);
				FHoudiniApi::SetParmStringValue(NodeId, ConvertedString.c_str(), ParmId, 0);
			}
		}

		FHoudiniApi::CookAsset(CurveAssetId, nullptr);

		// We need to update newly created curve.
		UpdateInputCurve();
	}

	bLoadedParameter = false;
	return Super::UploadParameterValue();
}


void
UHoudiniAssetInput::BeginDestroy()
{
	Super::BeginDestroy();
	ClearInputCurveParameters();
}


void
UHoudiniAssetInput::PostLoad()
{
	Super::PostLoad();

	if(InputCurve)
	{
		InputCurve->AttachTo(HoudiniAssetComponent, NAME_None, EAttachLocation::KeepRelativeOffset);
		InputCurve->SetVisibility(true);
		InputCurve->RegisterComponent();
		InputCurve->RemoveFromRoot();
		InputCurve->SetHoudiniAssetInput(this);
	}
}


void
UHoudiniAssetInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	// Serialize current choice selection.
	SerializeEnumeration(Ar, ChoiceIndex);

	// Create necessary widget resources.
	if(Ar.IsLoading())
	{
		CreateWidgetResources();
		bLoadedParameter = true;
	}

	// Serialize input index.
	Ar << InputIndex;

	// Serialize static mesh input.
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

	// Serialize curve.
	bool bCurveCreated = (InputCurve != nullptr);
	Ar << bCurveCreated;

	if(bCurveCreated)
	{
		if(Ar.IsLoading())
		{
			InputCurve = ConstructObject<UHoudiniSplineComponent>(UHoudiniSplineComponent::StaticClass(), HoudiniAssetComponent, NAME_None, RF_Transient);
			InputCurve->AddToRoot();
		}

		InputCurve->SerializeRaw(Ar);

		// Serialize curve parameters.
		SerializeInputCurveParameters(Ar);
	}
}


void
UHoudiniAssetInput::SerializeInputCurveParameters(FArchive& Ar)
{
	// Serialize number of parameters.
	int32 ParamCount = InputCurveParameters.Num();
	Ar << ParamCount;

	if(Ar.IsSaving())
	{
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(InputCurveParameters); IterParams; ++IterParams)
		{
			FString ParameterKey = IterParams.Key();
			UHoudiniAssetParameter* Parameter = IterParams.Value();

			Ar << ParameterKey;
			Ar << Parameter;
		}
	}
	else if(Ar.IsLoading())
	{
		InputCurveParameters.Empty();

		for(int32 ParmIdx = 0; ParmIdx < ParamCount; ++ParmIdx)
		{
			FString ParameterKey = TEXT("");
			UHoudiniAssetParameter* Parameter = nullptr;

			Ar << ParameterKey;
			Ar << Parameter;

			Parameter->SetHoudiniAssetComponent(nullptr);
			Parameter->SetParentParameter(this);

			InputCurveParameters.Add(ParameterKey, Parameter);
		}
	}
}


void
UHoudiniAssetInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInput* HoudiniAssetInput = Cast<UHoudiniAssetInput>(InThis);
	if(HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill())
	{
		// Add reference to held geometry object.
		if(HoudiniAssetInput->InputObject)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputObject, InThis);
		}

		// Add reference to held curve object.
		if(HoudiniAssetInput->InputCurve)
		{
			Collector.AddReferencedObject(HoudiniAssetInput->InputCurve, InThis);
		}

		// Add references for all curve input parameters.
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetInput->InputCurveParameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			Collector.AddReferencedObject(HoudiniAssetParameter, InThis);
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
		HoudiniAssetParameter->ConditionalBeginDestroy();
	}

	InputCurveParameters.Empty();
}


void
UHoudiniAssetInput::OnStaticMeshDropped(UObject* Object)
{
	if(Object != InputObject)
	{
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

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(InputObject), true, AllowedClasses, NewAssetFactories,
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


HAPI_AssetId
UHoudiniAssetInput::GetConnectedAssetId() const
{
	return ConnectedAssetId;
}


HAPI_AssetId
UHoudiniAssetInput::GetCurveAssetId() const
{
	return CurveAssetId;
}


HAPI_AssetId
UHoudiniAssetInput::GetGeometryAssetId() const
{
	return GeometryAssetId;
}


bool
UHoudiniAssetInput::IsGeometryAssetConnected() const
{
	return GeometryAssetId == ConnectedAssetId;
}


bool
UHoudiniAssetInput::IsCurveAssetConnected() const
{
	return CurveAssetId == ConnectedAssetId;
}


void
UHoudiniAssetInput::OnInputCurveChanged()
{
	MarkPreChanged();
	MarkChanged();
}


void
UHoudiniAssetInput::NotifyChildParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	if(HoudiniAssetParameter && EHoudiniAssetInputType::CurveInput == ChoiceIndex)
	{
		MarkPreChanged();

		if(FHoudiniEngineUtils::IsValidAssetId(CurveAssetId))
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
	if(FHoudiniEngineUtils::HapiGetNodeId(CurveAssetId, 0, 0, NodeId))
	{
		FHoudiniEngineUtils::HapiGetParameterDataAsString(NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT(""), CurvePointsString);
		FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_TYPE, (int32) EHoudiniSplineComponentType::Bezier, (int32&) CurveTypeValue);
		FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_METHOD, (int32) EHoudiniSplineComponentMethod::CVs, (int32&) CurveMethodValue);
		FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed);
	}

	// Construct geo part object.
	FHoudiniGeoPartObject HoudiniGeoPartObject(CurveAssetId, 0, 0, 0);
	HoudiniGeoPartObject.bIsCurve = true;

	HAPI_AttributeInfo AttributeRefinedCurvePositions;
	TArray<float> RefinedCurvePositions;
	FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION, AttributeRefinedCurvePositions, RefinedCurvePositions);

	// Process coords string and extract positions.
	TArray<FVector> CurvePoints;
	FHoudiniEngineUtils::ExtractStringPositions(CurvePointsString, CurvePoints);

	TArray<FVector> CurveDisplayPoints;
	FHoudiniEngineUtils::ConvertScaleAndFlipVectorData(RefinedCurvePositions, CurveDisplayPoints);

	InputCurve->Construct(HoudiniGeoPartObject, CurvePoints, CurveDisplayPoints, CurveTypeValue, CurveMethodValue, (CurveClosed == 1));

	// We also need to construct curve parameters we care about.
	TMap<FString, UHoudiniAssetParameter*> NewInputCurveParameters;

	{
		HAPI_NodeInfo NodeInfo;
		HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetNodeInfo(NodeId, &NodeInfo), false);

		TArray<HAPI_ParmInfo> ParmInfos;
		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParameters(NodeId, &ParmInfos[0], 0, NodeInfo.parmCount), false);

		// Retrieve integer values for this asset.
		TArray<int32> ParmValueInts;
		ParmValueInts.SetNumZeroed(NodeInfo.parmIntValueCount);
		if(NodeInfo.parmIntValueCount > 0)
		{
			HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParmIntValues(NodeId, &ParmValueInts[0], 0, NodeInfo.parmIntValueCount), false);
		}

		// Retrieve float values for this asset.
		TArray<float> ParmValueFloats;
		ParmValueFloats.SetNumZeroed(NodeInfo.parmFloatValueCount);
		if(NodeInfo.parmFloatValueCount > 0)
		{
			HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParmFloatValues(NodeId, &ParmValueFloats[0], 0, NodeInfo.parmFloatValueCount), false);
		}

		// Retrieve string values for this asset.
		TArray<HAPI_StringHandle> ParmValueStrings;
		ParmValueStrings.SetNumZeroed(NodeInfo.parmStringValueCount);
		if(NodeInfo.parmStringValueCount > 0)
		{
			HOUDINI_CHECK_ERROR_EXECUTE_RETURN(FHoudiniApi::GetParmStringValues(NodeId, true, &ParmValueStrings[0], 0, NodeInfo.parmStringValueCount), false);
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
			if(!UHoudiniAssetParameter::RetrieveParameterName(ParmInfo, ParameterName))
			{
				// We had trouble retrieving name of this parameter, skip it.
				continue;
			}

			// See if it's one of parameters we are interested in.
			if(!ParameterName.Equals(TEXT("method")) &&
			   !ParameterName.Equals(TEXT("type")) &&
			   !ParameterName.Equals(TEXT("close")))
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
		// We need to trigger details panel update.
		HoudiniAssetComponent->UpdateEditorProperties(false);
		bSwitchedToCurve = false;
	}
}


TSharedRef<SWidget>
UHoudiniAssetInput::CreateChoiceEntryWidget(TSharedPtr<FString> ChoiceEntry)
{
	return SNew(STextBlock)
		   .Text(*ChoiceEntry)
		   .ToolTipText(*ChoiceEntry)
		   .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}


void
UHoudiniAssetInput::OnChoiceChange(TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
{
	if(!NewChoice.IsValid())
	{
		return;
	}

	bool bChanged = false;
	ChoiceStringValue = *(NewChoice.Get());

	// We need to match selection based on label.
	int32 LabelIdx = 0;
	for(; LabelIdx < StringChoiceLabels.Num(); ++LabelIdx)
	{
		FString* ChoiceLabel = StringChoiceLabels[LabelIdx].Get();

		if(ChoiceLabel->Equals(ChoiceStringValue))
		{
			bChanged = true;
			break;
		}
	}

	if(bChanged)
	{
		// We are switching modes.
		ChoiceIndex = static_cast<EHoudiniAssetInputType::Enum>(LabelIdx);

		// We need to disconnect currently connected asset, if we have one.
		DisconnectInputAsset();

		if(EHoudiniAssetInputType::GeometryInput == ChoiceIndex)
		{
			// If we have spline, delete it.
			if(InputCurve)
			{
				InputCurve->DetachFromParent();
				InputCurve->UnregisterComponent();
				InputCurve->DestroyComponent();

				InputCurve = nullptr;
			}

			// We need to trigger details panel update.
			HoudiniAssetComponent->UpdateEditorProperties(false);
		}
		else if(EHoudiniAssetInputType::CurveInput == ChoiceIndex)
		{
			if(!InputCurve)
			{
				// Create new spline component.
				UHoudiniSplineComponent* HoudiniSplineComponent = ConstructObject<UHoudiniSplineComponent>(UHoudiniSplineComponent::StaticClass(), HoudiniAssetComponent, NAME_None, RF_Transient);
				HoudiniSplineComponent->AttachTo(HoudiniAssetComponent, NAME_None, EAttachLocation::KeepRelativeOffset);
				HoudiniSplineComponent->RegisterComponent();
				HoudiniSplineComponent->SetVisibility(true);
				HoudiniSplineComponent->SetHoudiniAssetInput(this);

				// Store this component as input curve.
				InputCurve = HoudiniSplineComponent;
			}

			bSwitchedToCurve = true;
		}

		// If we have input object and geometry asset, we need to connect it back.
		MarkPreChanged();
		ConnectInputAsset();
		MarkChanged();
	}
}


FString
UHoudiniAssetInput::HandleChoiceContentText() const
{
	return ChoiceStringValue;
}
