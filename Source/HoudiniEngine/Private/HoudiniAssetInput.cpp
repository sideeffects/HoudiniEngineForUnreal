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
	FText CurveText;

	if(InputObject && InputObject->IsA(UHoudiniSplineComponent::StaticClass()))
	{
		CurveText = FText::FromString(TEXT("Select Input Curve"));
	}
	else
	{
		CurveText = FText::FromString(TEXT("Create Input Curve"));
	}

	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(GetParameterLabel())
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	HorizontalBox->AddSlot()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(SAssetDropTarget)
				.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this, &UHoudiniAssetInput::OnAssetDropped))
				.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this, &UHoudiniAssetInput::OnIsAssetAcceptableForDrop))
				.ToolTipText(GetParameterLabel())
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().Padding(2, 2)
				[
					SAssignNew(InputWidget, SAssetSearchBox)
					.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UHoudiniAssetInput::SetValueCommitted))
				]
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnButtonClickedCreateSelectCurveInput))
			//.TextStyle(FEditorStyle::Get(), "ContentBrowser.Filters.Text")
			.Text(CurveText)
			.ToolTipText(CurveText)
		]
		+SVerticalBox::Slot()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnButtonClickedResetInput))
			//.TextStyle(FEditorStyle::Get(), "ContentBrowser.Filters.Text")
			.Text(LOCTEXT("ResetInput", "Reset Input"))
			.ToolTipText(LOCTEXT("HoudiniAssetInputTipResetInput", "Reset Input"))
		]
	];

	if(InputObject)
	{
		FString AssetName = InputObject->GetName();
		InputWidget->SetText(FText::FromString(AssetName));
	}

	Row.ValueWidget.Widget = HorizontalBox;
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
UHoudiniAssetInput::OnAssetDropped(UObject* Object)
{
	if(Object && (Object != InputObject))
	{
		MarkPreChanged();

		InputObject = Object;

		FString AssetName = InputObject->GetName();
		InputWidget->SetText(FText::FromString(AssetName));

		MarkChanged();
	}
}


bool
UHoudiniAssetInput::OnIsAssetAcceptableForDrop(const UObject* Object)
{
	// We will accept only static meshes at this point.
	if(Object && Object->IsA(UStaticMesh::StaticClass()))
	{
		return true;
	}

	return false;
}


void
UHoudiniAssetInput::SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType)
{
	FString AssetName = TEXT("");
	bool bChanged = false;

	if(InValue.IsEmpty())
	{
		MarkPreChanged();

		// Widget has been cleared.
		InputObject = nullptr;
		bChanged = true;
	}
	else
	{
		// Otherwise set back the old text.
		if(InputObject)
		{
			AssetName = InputObject->GetName();
		}
	}

	InputWidget->SetText(FText::FromString(AssetName));

	if(bChanged)
	{
		MarkChanged();
	}
}


FReply
UHoudiniAssetInput::OnButtonClickedResetInput()
{
	if(InputObject)
	{
		if(InputObject->IsA(UStaticMesh::StaticClass()))
		{
			// Static mesh is used as input.

			MarkPreChanged();

			InputObject = nullptr;
			InputWidget->SetText(FText::FromString(TEXT("")));

			MarkChanged();
		}
		else if(InputObject->IsA(UHoudiniSplineComponent::StaticClass()))
		{
			// We have curve input.
		}
	}

	return FReply::Handled();
}


FReply
UHoudiniAssetInput::OnButtonClickedCreateSelectCurveInput()
{
	return FReply::Handled();
}

