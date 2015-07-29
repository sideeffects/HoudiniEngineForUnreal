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
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetInstanceInputField.h"
#include "HoudiniApi.h"


UHoudiniAssetInstanceInput::UHoudiniAssetInstanceInput(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ObjectToInstanceId(-1),
	bIsAttributeInstancer(false)
{
	TupleSize = 0;
}


UHoudiniAssetInstanceInput::~UHoudiniAssetInstanceInput()
{

}


UHoudiniAssetInstanceInput*
UHoudiniAssetInstanceInput::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
	const FHoudiniGeoPartObject& InHoudiniGeoPartObject)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

	// Get name of this input. For the time being we only support geometry inputs.
	HAPI_ObjectInfo ObjectInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjects(InHoudiniAssetComponent->GetAssetId(), &ObjectInfo,
		InHoudiniGeoPartObject.ObjectId, 1))
	{
		return HoudiniAssetInstanceInput;
	}

	// If this is an attribute instancer, see if attribute exists.
	bool bAttributeCheck = UHoudiniAssetInstanceInput::CheckInstanceAttribute(InHoudiniAssetComponent->GetAssetId(),
		InHoudiniGeoPartObject);

	// This is invalid combination, no object to instance and input is not an attribute instancer.
	if(!bAttributeCheck && -1 == ObjectInfo.objectToInstanceId)
	{
		return HoudiniAssetInstanceInput;
	}

	HoudiniAssetInstanceInput = NewObject<UHoudiniAssetInstanceInput>(InHoudiniAssetComponent, 
		UHoudiniAssetInstanceInput::StaticClass(), NAME_None, RF_Public);

	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->SetNameAndLabel(ObjectInfo.nameSH);
	HoudiniAssetInstanceInput->HoudiniGeoPartObject = InHoudiniGeoPartObject;
	HoudiniAssetInstanceInput->ObjectToInstanceId = ObjectInfo.objectToInstanceId;

	// Check if this instancer is an attribute instancer and if it is, mark it as such.
	HoudiniAssetInstanceInput->bIsAttributeInstancer = bAttributeCheck;

	return HoudiniAssetInstanceInput;
}


bool
UHoudiniAssetInstanceInput::CreateInstanceInput()
{
	// Retrieve instance transforms (for each point).
	TArray<FTransform> AllTransforms;
	FHoudiniEngineUtils::HapiGetInstanceTransforms(HoudiniAssetComponent->GetAssetId(), HoudiniGeoPartObject.ObjectId,
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, AllTransforms);

	// List of new fields. Reused input fields will also be placed here.
	TArray<UHoudiniAssetInstanceInputField*> NewInstanceInputFields;

	if(bIsAttributeInstancer)
	{
		HAPI_AttributeInfo ResultAttributeInfo;
		TArray<FString> PointInstanceValues;

		if(!FHoudiniEngineUtils::HapiGetAttributeDataAsString(HoudiniAssetComponent->GetAssetId(), 
			HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, 
			HAPI_UNREAL_ATTRIB_INSTANCE, ResultAttributeInfo, PointInstanceValues))
		{
			// This should not happen - attribute exists, but there was an error retrieving it.
			return false;
		}

		// Instance attribute exists on points.

		// Number of points must match number of transforms.
		if(PointInstanceValues.Num() != AllTransforms.Num())
		{
			// This should not happen, we have mismatch between number of instance values and transforms.
			return false;
		}

		// Get unique names.
		TSet<FString> UniquePointInstanceValues(PointInstanceValues);
		
		// If instance attribute exists on points, we need to get all unique values.
		TMap<FString, TArray<FHoudiniGeoPartObject> > ObjectsToInstance;

		// For each name, we need to retrieve corresponding geo object parts as well as sequence of geo object parts.
		for(TSet<FString>::TIterator IterString(UniquePointInstanceValues); IterString; ++IterString)
		{
			const FString& UniqueName = *IterString;
			HoudiniAssetComponent->LocateStaticMeshes(UniqueName, ObjectsToInstance);
		}

		if(0 == ObjectsToInstance.Num())
		{
			// We have no objects to instance.
			return false;
		}

		for(TMap<FString, TArray<FHoudiniGeoPartObject> >::TIterator IterInstanceObjects(ObjectsToInstance);
			IterInstanceObjects; ++IterInstanceObjects)
		{
			const FString& InstancePath = IterInstanceObjects.Key();
			TArray<FHoudiniGeoPartObject>& InstanceGeoPartObjects = IterInstanceObjects.Value();

			// Retrieve all applicable transforms for this object.
			TArray<FTransform> ObjectTransforms;
			GetPathInstaceTransforms(InstancePath, PointInstanceValues, AllTransforms, ObjectTransforms);
			check(ObjectTransforms.Num());

			for(int32 InstanceGeoPartObjectIdx = 0; InstanceGeoPartObjectIdx < InstanceGeoPartObjects.Num();
				++InstanceGeoPartObjectIdx)
			{
				const FHoudiniGeoPartObject& ItemHoudiniGeoPartObject = InstanceGeoPartObjects[InstanceGeoPartObjectIdx];

				// Locate or create an input field.
				CreateInstanceInputField(ItemHoudiniGeoPartObject, ObjectTransforms, InstancePath, InstanceInputFields,
					NewInstanceInputFields);
			}
		}
	}
	else
	{
		// Locate all geo objects requiring instancing (can be multiple if geo / part / object split took place).
		TArray<FHoudiniGeoPartObject> ObjectsToInstance;
		HoudiniAssetComponent->LocateStaticMeshes(ObjectToInstanceId, ObjectsToInstance);

		// Process each existing detected object that needs to be instanced.
		for(int32 GeoIdx = 0; GeoIdx < ObjectsToInstance.Num(); ++GeoIdx)
		{
			const FHoudiniGeoPartObject& ItemHoudiniGeoPartObject = ObjectsToInstance[GeoIdx];

			// Locate or create an input field.
			CreateInstanceInputField(ItemHoudiniGeoPartObject, AllTransforms, TEXT(""), InstanceInputFields,
				NewInstanceInputFields);
		}
	}

	// Sort and store new fields.
	NewInstanceInputFields.Sort(FHoudiniAssetInstanceInputFieldSortPredicate());
	InstanceInputFields = NewInstanceInputFields;

	return true;
}


UHoudiniAssetInstanceInputField*
UHoudiniAssetInstanceInput::LocateInputField(const FHoudiniGeoPartObject& GeoPartObject, const FString& InstancePathName)
{
	UHoudiniAssetInstanceInputField* FoundHoudiniAssetInstanceInputField = nullptr;

	for(int32 FieldIdx = 0; FieldIdx < InstanceInputFields.Num(); ++FieldIdx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[FieldIdx];

		if(HoudiniAssetInstanceInputField->InstancePathName == InstancePathName &&
			HoudiniAssetInstanceInputField->HoudiniGeoPartObject == GeoPartObject)
		{
			FoundHoudiniAssetInstanceInputField = HoudiniAssetInstanceInputField;
			break;
		}
	}

	return FoundHoudiniAssetInstanceInputField;
}


void
UHoudiniAssetInstanceInput::CreateInstanceInputField(const FHoudiniGeoPartObject& InHoudiniGeoPartObject,
	const TArray<FTransform>& ObjectTransforms, const FString& InstancePathName,
	const TArray<UHoudiniAssetInstanceInputField*>& OldInstanceInputFields,
	TArray<UHoudiniAssetInstanceInputField*>& NewInstanceInputFields)
{
	// Locate static mesh for this geo part.
	UStaticMesh* StaticMesh = HoudiniAssetComponent->LocateStaticMesh(InHoudiniGeoPartObject);
	check(StaticMesh);

	// Locate corresponding input field.
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField =
		LocateInputField(InHoudiniGeoPartObject, InstancePathName);

	if(!HoudiniAssetInstanceInputField)
	{
		// Input field does not exist, we need to create it.
		HoudiniAssetInstanceInputField = UHoudiniAssetInstanceInputField::Create(HoudiniAssetComponent,
			InHoudiniGeoPartObject, InstancePathName);

		// Assign original and static mesh.
		HoudiniAssetInstanceInputField->OriginalStaticMesh = StaticMesh;
		HoudiniAssetInstanceInputField->StaticMesh = StaticMesh;

		// Create instanced component.
		HoudiniAssetInstanceInputField->CreateInstancedComponent();
	}
	else
	{
		// Remove item from old list.
		InstanceInputFields.RemoveSingleSwap(HoudiniAssetInstanceInputField, false);

		if(HoudiniAssetInstanceInputField->OriginalStaticMesh == HoudiniAssetInstanceInputField->StaticMesh)
		{
			// Assign original and static mesh.
			HoudiniAssetInstanceInputField->OriginalStaticMesh = StaticMesh;
			HoudiniAssetInstanceInputField->StaticMesh = StaticMesh;
		}
	}

	// Update component transformation.
	HoudiniAssetInstanceInputField->UpdateRelativeTransform();

	// Set transforms for this input.
	HoudiniAssetInstanceInputField->SetInstanceTransforms(ObjectTransforms);

	// Add field to list of fields.
	NewInstanceInputFields.Add(HoudiniAssetInstanceInputField);
}


bool
UHoudiniAssetInstanceInput::CreateInstanceInputPostLoad()
{
	

	// UpdateRelativeTransform

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		HoudiniAssetInstanceInputField->CreateInstancedComponent();
	}

	int i =32;

	/*
	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		// Get geo part information for this index.
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = SerializedGeoPartObjects[Idx];

		UInstancedStaticMeshComponent* Component =
			NewObject<UInstancedStaticMeshComponent>(HoudiniAssetComponent->GetOwner(),
				UInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transient);

		Component->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);
		Component->AttachTo(HoudiniAssetComponent);

		SetComponentInstanceTransformations(Component, InstancedTransforms[Idx], Idx);
		InstancedStaticMeshComponents[Idx] = Component;

		// Locate default / original mesh.
		OriginalStaticMeshes[Idx] = HoudiniAssetComponent->LocateStaticMesh(HoudiniGeoPartObject);

		// If custom mesh is not used, use original.
		if(!StaticMeshes[Idx])
		{
			StaticMeshes[Idx] = OriginalStaticMeshes[Idx];
		}

		if(HoudiniGeoPartObject.bIsCollidable)
		{
			// We want to make it invisible if this is a collision instancer.
			Component->SetVisibility(false);
		}
		else
		{
			Component->SetVisibility(true);
		}

		// Set mesh for this component.
		Component->SetStaticMesh(StaticMeshes[Idx]);

		Component->RegisterComponent();
		Component->GetBodyInstance()->bAutoWeld = false;
	}

	SerializedGeoPartObjects.Empty();
	*/
	return true;
}


void
UHoudiniAssetInstanceInput::RecreateRenderStates()
{
	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		HoudiniAssetInstanceInputField->RecreateRenderState();
	}
}


void
UHoudiniAssetInstanceInput::RecreatePhysicsStates()
{
	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		HoudiniAssetInstanceInputField->RecreatePhysicsState();
	}
}


bool
UHoudiniAssetInstanceInput::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
	UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// This implementation is not a true parameter. This method should not be called.
	check(false);
	return false;
}


#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	CachedComboButtons.Empty();
	CachedThumbnailBorders.Empty();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->GetStaticMesh();

		FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
		FText LabelText =
			FText::Format(LOCTEXT("HoudiniStaticMeshInstanceInput", "Static Mesh Instance {0}"),
				FText::AsNumber(Idx));

		Row.NameWidget.Widget = SNew(STextBlock)
								.Text(LabelText)
								.ToolTipText(LabelText)
								.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		// Create thumbnail for this mesh.
		TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(new FAssetThumbnail(StaticMesh, 64, 64,
			AssetThumbnailPool));
		TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
		TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
		TSharedPtr<SBorder> StaticMeshThumbnailBorder;

		VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
		[
			SNew(SAssetDropTarget)
			.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this,
				&UHoudiniAssetInstanceInput::OnStaticMeshDraggedOver))
			.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this,
				&UHoudiniAssetInstanceInput::OnStaticMeshDropped, HoudiniAssetInstanceInputField, Idx))
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
		[
			SAssignNew(StaticMeshThumbnailBorder, SBorder)
			.Padding(5.0f)

			.BorderImage(TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder, HoudiniAssetInstanceInputField, Idx)))
			.OnMouseDoubleClick(FPointerEventHandler::CreateUObject(this,
				&UHoudiniAssetInstanceInput::OnThumbnailDoubleClick, (UObject*) StaticMesh))
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(StaticMesh->GetPathName()))
				[
					StaticMeshThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail border for this static mesh.
		//HoudiniAssetInstanceInputField->AssignThumbnailBorder(StaticMeshThumbnailBorder);
		CachedThumbnailBorders.Add(HoudiniAssetInstanceInputField, StaticMeshThumbnailBorder);

		TSharedPtr<SComboButton> AssetComboButton;
		TSharedPtr<SHorizontalBox> ButtonBox;

		/*
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
					//.ToolTipText(this, &FHoudiniAssetComponentDetails::OnGetToolTip )
					.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
					.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
					.OnGetMenuContent(FOnGetContent::CreateUObject(this,
						&UHoudiniAssetInstanceInput::OnGetStaticMeshMenuContent, HoudiniAssetInstanceInputField, Idx))
					.ContentPadding(2.0f)
					.ButtonContent()
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
						.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
						.Text(FText::FromString(StaticMesh->GetName()))
					]
				]
			]
		];

		// Store combobutton for this static mesh.
		//HoudiniAssetInstanceInputField->AssignComboButton(AssetComboButton);
		CachedComboButtons.Add(HoudiniAssetInstanceInputField, AssetComboButton);

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(StaticMesh->GetName()));
		FText StaticMeshTooltip =
			FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"),
				Args);

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInstanceInput::OnStaticMeshBrowse, StaticMesh),
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
			.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInstanceInput::OnResetStaticMeshClicked,
				HoudiniAssetInstanceInputField, Idx))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];
		*/

		FText LabelRotationText = LOCTEXT("HoudiniRotationOffset", "Rotation Offset:");
		VerticalBox->AddSlot().Padding(5, 2).AutoHeight()
		[
			SNew(STextBlock)
			.Text(LabelRotationText)
			.ToolTipText(LabelRotationText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

		VerticalBox->AddSlot().Padding(5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				SNew(SRotatorInputBox)
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.Roll(TAttribute<TOptional<float> >::Create(
					TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationRoll, HoudiniAssetInstanceInputField)))
				.Pitch(TAttribute<TOptional<float> >::Create(
					TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationPitch, HoudiniAssetInstanceInputField)))
				.Yaw(TAttribute<TOptional<float> >::Create(
					TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationYaw, HoudiniAssetInstanceInputField)))
				.OnRollChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetRotationRoll, HoudiniAssetInstanceInputField))
				.OnPitchChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetRotationPitch, HoudiniAssetInstanceInputField))
				.OnYawChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetRotationYaw, HoudiniAssetInstanceInputField))
			]
		];

		FText LabelScaleText = LOCTEXT("HoudiniScaleOffset", "Scale Offset:");
		VerticalBox->AddSlot().Padding(5, 2).AutoHeight()
		[
			SNew(STextBlock)
			.Text(LabelScaleText)
			.ToolTipText(LabelScaleText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

		VerticalBox->AddSlot().Padding(5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				SNew(SVectorInputBox)
				.bColorAxisLabels(true)
				.X(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::GetScaleX, HoudiniAssetInstanceInputField)))
				.Y(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::GetScaleY, HoudiniAssetInstanceInputField)))
				.Z(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::GetScaleZ, HoudiniAssetInstanceInputField)))
				.OnXChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetScaleX, HoudiniAssetInstanceInputField))
				.OnYChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetScaleY, HoudiniAssetInstanceInputField))
				.OnZChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetScaleZ, HoudiniAssetInstanceInputField))
			]
		];

		FText LabelLinearScaleText = LOCTEXT("HoudiniScaleFieldsLinearly", "Scale all fields linearly");
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
				&UHoudiniAssetInstanceInput::CheckStateChanged, HoudiniAssetInstanceInputField))
			.IsChecked(TAttribute<ECheckBoxState>::Create(
				TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::IsChecked, HoudiniAssetInstanceInputField)))
			.Content()
			[
				SNew(STextBlock)
				.Text(LabelLinearScaleText)
				.ToolTipText(LabelLinearScaleText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		Row.ValueWidget.Widget = VerticalBox;
		Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	}
}

#endif


bool
UHoudiniAssetInstanceInput::UploadParameterValue()
{
	return Super::UploadParameterValue();
}


void
UHoudiniAssetInstanceInput::BeginDestroy()
{
	Super::BeginDestroy();
}


void
UHoudiniAssetInstanceInput::SetHoudiniAssetComponent(UHoudiniAssetComponent* InHoudiniAssetComponent)
{
	UHoudiniAssetParameter::SetHoudiniAssetComponent(InHoudiniAssetComponent);

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		InstanceInputFields[Idx]->HoudiniAssetComponent = InHoudiniAssetComponent;
	}
}


void
UHoudiniAssetInstanceInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << HoudiniAssetInstanceInputFlagsPacked;
	HoudiniGeoPartObject.Serialize(Ar);
	Ar << ObjectToInstanceId;

	// Serialize number of fields.
	int32 NumFields = InstanceInputFields.Num();
	Ar << NumFields;

	for(int32 Idx = 0; Idx < NumFields; ++Idx)
	{
		if(Ar.IsLoading())
		{
			UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = nullptr;
			Ar << HoudiniAssetInstanceInputField;
			InstanceInputFields.Add(HoudiniAssetInstanceInputField);
		}
		else if(Ar.IsSaving())
		{
			Ar << InstanceInputFields[Idx];
		}
	}
}


void
UHoudiniAssetInstanceInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Cast<UHoudiniAssetInstanceInput>(InThis);
	if(HoudiniAssetInstanceInput && !HoudiniAssetInstanceInput->IsPendingKill())
	{
		// Add references to all used fields.
		for(int32 Idx = 0; Idx < HoudiniAssetInstanceInput->InstanceInputFields.Num(); ++Idx)
		{
			UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = 
				HoudiniAssetInstanceInput->InstanceInputFields[Idx];

			Collector.AddReferencedObject(HoudiniAssetInstanceInputField, InThis);

			/*
			UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->GetStaticMesh();
			if(StaticMesh)
			{
				Collector.AddReferencedObject(StaticMesh, InThis);
			}
			*/
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


bool
UHoudiniAssetInstanceInput::IsAttributeInstancer() const
{
	return bIsAttributeInstancer;
}


bool
UHoudiniAssetInstanceInput::IsObjectInstancer() const
{
	return (-1 != ObjectToInstanceId);
}


#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::CloneComponentsAndAttachToActor(AActor* Actor)
{
	USceneComponent* RootComponent = Actor->GetRootComponent();

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		UStaticMesh* OutStaticMesh = nullptr;

		UInstancedStaticMeshComponent* InstancedStaticMeshComponent = 
			HoudiniAssetInstanceInputField->GetInstancedStaticMeshComponent();

		// If original static mesh is used, then we need to bake it.
		if(HoudiniAssetInstanceInputField->IsOriginalStaticMeshUsed())
		{
			const FHoudiniGeoPartObject& ItemHoudiniGeoPartObject =
				HoudiniAssetComponent->LocateGeoPartObject(HoudiniAssetInstanceInputField->GetStaticMesh());

			// Bake the referenced static mesh.
			OutStaticMesh =
				FHoudiniEngineUtils::BakeStaticMesh(HoudiniAssetComponent, ItemHoudiniGeoPartObject, 
					HoudiniAssetInstanceInputField->GetOriginalStaticMesh());

			if(OutStaticMesh)
			{
				FAssetRegistryModule::AssetCreated(OutStaticMesh);
			}
			else
			{
				continue;
			}
		}
		else
		{
			OutStaticMesh = HoudiniAssetInstanceInputField->GetStaticMesh();
		}

		UInstancedStaticMeshComponent* DuplicatedComponent =
			NewObject<UInstancedStaticMeshComponent>(Actor, UInstancedStaticMeshComponent::StaticClass(), NAME_None, 
				RF_Public);

		Actor->AddInstanceComponent(DuplicatedComponent);
		DuplicatedComponent->SetStaticMesh(OutStaticMesh);

		// Set component instances.
		{
			FRotator RotationOffset(0.0f, 0.0f, 0.0f);
			FVector ScaleOffset(1.0f, 1.0f, 1.0f);

			const TArray<FTransform>& InstancedTransforms = HoudiniAssetInstanceInputField->GetInstancedTransforms();

			FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(DuplicatedComponent, InstancedTransforms,
				RotationOffset, ScaleOffset);
		}

		// Copy visibility.
		DuplicatedComponent->SetVisibility(InstancedStaticMeshComponent->IsVisible());

		DuplicatedComponent->AttachTo(RootComponent);
		DuplicatedComponent->RegisterComponent();
		DuplicatedComponent->GetBodyInstance()->bAutoWeld = false;
	}
}

#endif


void
UHoudiniAssetInstanceInput::GetPathInstaceTransforms(const FString& ObjectInstancePath,
	const TArray<FString>& PointInstanceValues, const TArray<FTransform>& Transforms,
	TArray<FTransform>& OutTransforms)
{
	OutTransforms.Empty();

	for(int32 Idx = 0; Idx < PointInstanceValues.Num(); ++Idx)
	{
		if(ObjectInstancePath.Equals(PointInstanceValues[Idx]))
		{
			OutTransforms.Add(Transforms[Idx]);
		}
	}
}


bool
UHoudiniAssetInstanceInput::CheckInstanceAttribute(HAPI_AssetId InAssetId, const FHoudiniGeoPartObject& GeoPartObject)
{
	if(-1 == InAssetId || -1 == GeoPartObject.ObjectId || -1 == GeoPartObject.GeoId || -1 == GeoPartObject.PartId)
	{
		return false;
	}

	return FHoudiniEngineUtils::HapiCheckAttributeExists(InAssetId, GeoPartObject.ObjectId, 
		GeoPartObject.GeoId, GeoPartObject.PartId, HAPI_UNREAL_ATTRIB_INSTANCE, HAPI_ATTROWNER_POINT);
}


#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::OnStaticMeshDropped(UObject* InObject, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx)
{
	UStaticMesh* InputStaticMesh = Cast<UStaticMesh>(InObject);
	UStaticMesh* UsedStaticMesh = HoudiniAssetInstanceInputField->GetStaticMesh();

	if(InputStaticMesh && UsedStaticMesh != InputStaticMesh)
	{
		HoudiniAssetInstanceInputField->SetStaticMesh(InputStaticMesh);
	}
}


const FSlateBrush*
UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 Idx) const
{
	TSharedPtr<SBorder> ThumbnailBorder = CachedThumbnailBorders[HoudiniAssetInstanceInputField];
	if(ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
}


bool
UHoudiniAssetInstanceInput::OnStaticMeshDraggedOver(const UObject* Object) const
{
	// We will accept only static meshes as inputs to instancers.
	if(Object && Object->IsA(UStaticMesh::StaticClass()))
	{
		return true;
	}

	return false;
}


FReply
UHoudiniAssetInstanceInput::OnThumbnailDoubleClick(const FGeometry& InMyGeometry,
	const FPointerEvent& InMouseEvent, UObject* Object)
{
	if(Object && GEditor)
	{
		GEditor->EditObject(Object);
	}

	return FReply::Handled();
}


TSharedRef<SWidget>
UHoudiniAssetInstanceInput::OnGetStaticMeshMenuContent(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 Idx)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UStaticMesh::StaticClass());

	TArray<UFactory*> NewAssetFactories;

	UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->GetStaticMesh();

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(StaticMesh), true,
		AllowedClasses, NewAssetFactories,OnShouldFilterStaticMesh,
		FOnAssetSelected::CreateUObject(this, &UHoudiniAssetInstanceInput::OnStaticMeshSelected,
			HoudiniAssetInstanceInputField, Idx),
		FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInstanceInput::CloseStaticMeshComboButton));
}


void
UHoudiniAssetInstanceInput::OnStaticMeshBrowse(UStaticMesh* StaticMesh)
{
	if(GEditor)
	{
		TArray<UObject*> Objects;
		Objects.Add(StaticMesh);
		GEditor->SyncBrowserToObjects(Objects);
	}
}


FReply
UHoudiniAssetInstanceInput::OnResetStaticMeshClicked(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 Idx)
{
	UStaticMesh* OriginalStaticMesh = HoudiniAssetInstanceInputField->GetOriginalStaticMesh();
	OnStaticMeshDropped(OriginalStaticMesh, HoudiniAssetInstanceInputField, Idx);

	return FReply::Handled();
}


void
UHoudiniAssetInstanceInput::CloseStaticMeshComboButton()
{

}


void
UHoudiniAssetInstanceInput::OnStaticMeshSelected(const FAssetData& AssetData,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx)
{
	TSharedPtr<SComboButton> AssetComboButton = CachedComboButtons[HoudiniAssetInstanceInputField];
	if(AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnStaticMeshDropped(Object, HoudiniAssetInstanceInputField, Idx);
	}
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationRoll(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset();
	return Rotator.Roll;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationPitch(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset();
	return Rotator.Pitch;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationYaw(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset();
	return Rotator.Yaw;
}


void
UHoudiniAssetInstanceInput::SetRotationRoll(float Value, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset();
	Rotator.Roll = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetRotationPitch(float Value, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset();
	Rotator.Pitch = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetRotationYaw(float Value, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset();
	Rotator.Yaw = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleX(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	const FVector& Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset();
	return Scale3D.X;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleY(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	const FVector& Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset();
	return Scale3D.Y;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleZ(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	const FVector& Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset();
	return Scale3D.Z;
}


void
UHoudiniAssetInstanceInput::SetScaleX(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset();
	Scale3D.X = Value;

	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly())
	{
		Scale3D.Y = Value;
		Scale3D.Z = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetScaleY(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset();
	Scale3D.Y = Value;

	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly())
	{
		Scale3D.X = Value;
		Scale3D.Z = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetScaleZ(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset();
	Scale3D.Z = Value;

	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly())
	{
		Scale3D.Y = Value;
		Scale3D.X = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::CheckStateChanged(ECheckBoxState NewState,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField)
{
	HoudiniAssetInstanceInputField->SetLinearOffsetScale(ECheckBoxState::Checked == NewState);
}


ECheckBoxState
UHoudiniAssetInstanceInput::IsChecked(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField) const
{
	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly())
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

#endif


void
UHoudiniAssetInstanceInput::UpdateStaticMeshMaterial(UStaticMesh* OtherStaticMesh, int32 MaterialIdx,
	UMaterialInterface* MaterialInterface)
{
	/*
	for(int32 MeshIdx = 0; MeshIdx < StaticMeshes.Num(); ++MeshIdx)
	{
		UStaticMesh* StaticMesh = StaticMeshes[MeshIdx];

		if(StaticMesh == OtherStaticMesh)
		{
			UStaticMeshComponent* StaticMeshComponent = InstancedStaticMeshComponents[MeshIdx];
			StaticMeshComponent->SetMaterial(MaterialIdx, MaterialInterface);
		}
	}
	*/
}

