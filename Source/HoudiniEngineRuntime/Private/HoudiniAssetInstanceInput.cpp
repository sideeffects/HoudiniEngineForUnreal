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
	if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjects(
			FHoudiniEngine::Get().GetSession(),
			InHoudiniAssetComponent->GetAssetId(),
			&ObjectInfo,
			InHoudiniGeoPartObject.ObjectId,
			1
		)
	)
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
		UHoudiniAssetInstanceInput::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->SetNameAndLabel(ObjectInfo.nameSH);
	HoudiniAssetInstanceInput->HoudiniGeoPartObject = InHoudiniGeoPartObject;
	HoudiniAssetInstanceInput->ObjectToInstanceId = ObjectInfo.objectToInstanceId;

	// Check if this instancer is an attribute instancer and if it is, mark it as such.
	HoudiniAssetInstanceInput->bIsAttributeInstancer = bAttributeCheck;

	return HoudiniAssetInstanceInput;
}


UHoudiniAssetInstanceInput*
UHoudiniAssetInstanceInput::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, 
	UHoudiniAssetInstanceInput* InstanceInput)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = 
		DuplicateObject(InstanceInput, InHoudiniAssetComponent);

	// We need to duplicate fields.
	TArray<UHoudiniAssetInstanceInputField*> DuplicatedFields;

	for(int32 FieldIdx = 0; FieldIdx < HoudiniAssetInstanceInput->InstanceInputFields.Num(); ++FieldIdx)
	{
		// Get field at this index.
		UHoudiniAssetInstanceInputField* OriginalField = HoudiniAssetInstanceInput->InstanceInputFields[FieldIdx];
		UHoudiniAssetInstanceInputField* DuplicatedField = 
			UHoudiniAssetInstanceInputField::Create(InHoudiniAssetComponent, OriginalField);

		DuplicatedFields.Add(DuplicatedField);
	}

	// Set duplicated fields.
	HoudiniAssetInstanceInput->InstanceInputFields = DuplicatedFields;

	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->ParameterLabel = InstanceInput->ParameterLabel;
	HoudiniAssetInstanceInput->ParameterName = InstanceInput->ParameterName;
	HoudiniAssetInstanceInput->HoudiniGeoPartObject = InstanceInput->HoudiniGeoPartObject;
	HoudiniAssetInstanceInput->ObjectToInstanceId = InstanceInput->ObjectToInstanceId;
	HoudiniAssetInstanceInput->bIsAttributeInstancer = InstanceInput->bIsAttributeInstancer;

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
		HoudiniAssetInstanceInputField->AddInstanceVariation(StaticMesh);

	}
	else
	{
		// Remove item from old list.
		InstanceInputFields.RemoveSingleSwap(HoudiniAssetInstanceInputField, false);

		TArray<int> MatchingIndices;
		HoudiniAssetInstanceInputField->FindStaticMeshIndices(
								HoudiniAssetInstanceInputField->OriginalStaticMesh,
								MatchingIndices);
		for (int Idx = 0; Idx < MatchingIndices.Num(); Idx++)
		{
			int ReplacementIndex = MatchingIndices[Idx];
			HoudiniAssetInstanceInputField->ReplaceInstanceVariation(StaticMesh, ReplacementIndex);
		}

		HoudiniAssetInstanceInputField->OriginalStaticMesh = StaticMesh;
	}

	// Update component transformation.
	HoudiniAssetInstanceInputField->UpdateRelativeTransform();

	// Set transforms for this input.
	HoudiniAssetInstanceInputField->SetInstanceTransforms(ObjectTransforms);

	// Add field to list of fields.
	NewInstanceInputFields.Add(HoudiniAssetInstanceInputField);
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


FReply
UHoudiniAssetInstanceInput::OnAddInstanceVariation( 
		UHoudiniAssetInstanceInputField * InstanceInputField,
		int32 Index)
{   
	UStaticMesh * StaticMesh =  InstanceInputField->GetInstanceVariation(Index);
	InstanceInputField->AddInstanceVariation( StaticMesh );

	if (HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}

	return FReply::Handled();
}

FReply
UHoudiniAssetInstanceInput::OnRemoveInstanceVariation()
{
	check(false);
	return FReply::Handled();
}

void
UHoudiniAssetInstanceInput::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		for (int32 VariationIdx = 0; VariationIdx < HoudiniAssetInstanceInputField->InstanceVariationCount(); VariationIdx++)
		{			
			UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->GetInstanceVariation(VariationIdx);

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
					&UHoudiniAssetInstanceInput::OnStaticMeshDropped, HoudiniAssetInstanceInputField, Idx, VariationIdx))
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
					&UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder, HoudiniAssetInstanceInputField, Idx, VariationIdx)))
					.OnMouseDoubleClick(FPointerEventHandler::CreateUObject(this,
					&UHoudiniAssetInstanceInput::OnThumbnailDoubleClick, (UObject*)StaticMesh))
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


			/*
			HorizontalBox->AddSlot().Padding(0, 25.0f, 0, 25.0f).MaxWidth(25.0f)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("HoudiniEngine", "HEngineAddInstanceVariation", "+"))
					.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInstanceInput::OnAddInstanceVariation,
					HoudiniAssetInstanceInputField,VariationIdx))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
				];


			HorizontalBox->AddSlot().Padding(0.0f, 25.0f, 0.0f, 25.0f).MaxWidth(25.0f)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("HoudiniEngine", "HEngineSubInstanceVariation", "-"))
					.OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInstanceInput::OnRemoveInstanceVariation))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
				];			
			*/

			// Store thumbnail border for this static mesh.
			HoudiniAssetInstanceInputField->AssignThumbnailBorder(StaticMeshThumbnailBorder);

			TSharedPtr<SComboButton> AssetComboButton;
			TSharedPtr<SHorizontalBox> ButtonBox;

			HorizontalBox->AddSlot()
				.FillWidth(1.0f)
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ButtonBox, SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							SAssignNew(AssetComboButton, SComboButton)
							//.ToolTipText(this, &FHoudiniAssetComponentDetails::OnGetToolTip )
							.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
							.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
							.OnGetMenuContent(FOnGetContent::CreateUObject(this,
							&UHoudiniAssetInstanceInput::OnGetStaticMeshMenuContent, HoudiniAssetInstanceInputField, Idx, VariationIdx))
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
			HoudiniAssetInstanceInputField->AssignComboButton(AssetComboButton);

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
					HoudiniAssetInstanceInputField, Idx, VariationIdx))
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				];

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
					+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SRotatorInputBox)
						.AllowSpin(true)
						.bColorAxisLabels(true)
						.Roll(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationRoll, HoudiniAssetInstanceInputField, VariationIdx)))
						.Pitch(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationPitch, HoudiniAssetInstanceInputField, VariationIdx)))
						.Yaw(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationYaw, HoudiniAssetInstanceInputField, VariationIdx)))
						.OnRollChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetRotationRoll, HoudiniAssetInstanceInputField, VariationIdx))
						.OnPitchChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetRotationPitch, HoudiniAssetInstanceInputField, VariationIdx))
						.OnYawChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetRotationYaw, HoudiniAssetInstanceInputField, VariationIdx))
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
					+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SVectorInputBox)
						.bColorAxisLabels(true)
						.X(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetScaleX, HoudiniAssetInstanceInputField, VariationIdx)))
						.Y(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetScaleY, HoudiniAssetInstanceInputField, VariationIdx)))
						.Z(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetScaleZ, HoudiniAssetInstanceInputField, VariationIdx)))
						.OnXChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetScaleX, HoudiniAssetInstanceInputField, VariationIdx))
						.OnYChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetScaleY, HoudiniAssetInstanceInputField, VariationIdx))
						.OnZChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetScaleZ, HoudiniAssetInstanceInputField, VariationIdx))
					]
				];

			FText LabelLinearScaleText = LOCTEXT("HoudiniScaleFieldsLinearly", "Scale all fields linearly");
			VerticalBox->AddSlot().Padding(2, 2, 5, 2)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::CheckStateChanged, HoudiniAssetInstanceInputField, VariationIdx))
					.IsChecked(TAttribute<ECheckBoxState>::Create(
					TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::IsChecked, HoudiniAssetInstanceInputField, VariationIdx)))
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
	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		InstanceInputFields[Idx]->ConditionalBeginDestroy();
	}

	InstanceInputFields.Empty();

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

	// Serialize fields.
	Ar << InstanceInputFields;
}


void
UHoudiniAssetInstanceInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Cast<UHoudiniAssetInstanceInput>(InThis);
	if(HoudiniAssetInstanceInput)// && !HoudiniAssetInstanceInput->IsPendingKill())
	{
		// Add references to all used fields.
		for(int32 Idx = 0; Idx < HoudiniAssetInstanceInput->InstanceInputFields.Num(); ++Idx)
		{
			UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = 
				HoudiniAssetInstanceInput->InstanceInputFields[Idx];

			Collector.AddReferencedObject(HoudiniAssetInstanceInputField, InThis);
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
				//FIXME: Get rid of the hard coded index 0
				HoudiniAssetComponent->LocateGeoPartObject(HoudiniAssetInstanceInputField->GetInstanceVariation(0));

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
			//FIXME: Get rid of the hard coded index 0
			OutStaticMesh = HoudiniAssetInstanceInputField->GetInstanceVariation(0);
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
												UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, 
												int32 Idx, 
												int32 VariationIdx)
{
	UStaticMesh* InputStaticMesh = Cast<UStaticMesh>(InObject);	
	UStaticMesh* UsedStaticMesh = HoudiniAssetInstanceInputField->GetInstanceVariation(VariationIdx);

	if(InputStaticMesh && UsedStaticMesh != InputStaticMesh)
	{
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
		HoudiniAssetInstanceInputField->Modify();

		HoudiniAssetInstanceInputField->ReplaceInstanceVariation(InputStaticMesh,VariationIdx);		

		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->UpdateEditorProperties(false);
		}
		
	}
}


const FSlateBrush*
UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 Idx, int32 VariationIdx) const
{
	TSharedPtr<SBorder> ThumbnailBorder = HoudiniAssetInstanceInputField->GetThumbnailBorder();
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
	int32 Idx, int32 VariationIdx)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UStaticMesh::StaticClass());

	TArray<UFactory*> NewAssetFactories;

	//FIXME: Get rid of the hard coded index 0
	UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->GetInstanceVariation(0);

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(StaticMesh), true,
		AllowedClasses, NewAssetFactories,OnShouldFilterStaticMesh,
		FOnAssetSelected::CreateUObject(this, &UHoudiniAssetInstanceInput::OnStaticMeshSelected,
			HoudiniAssetInstanceInputField, Idx, VariationIdx),
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
	int32 Idx,
	int32 VariationIdx)
{
	UStaticMesh* OriginalStaticMesh = HoudiniAssetInstanceInputField->GetOriginalStaticMesh();
	OnStaticMeshDropped(OriginalStaticMesh, HoudiniAssetInstanceInputField, Idx, VariationIdx);

	return FReply::Handled();
}


void
UHoudiniAssetInstanceInput::CloseStaticMeshComboButton()
{

}


void
UHoudiniAssetInstanceInput::OnStaticMeshSelected(const FAssetData& AssetData,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx,
	int32 VariationIdx)
{
	TSharedPtr<SComboButton> AssetComboButton = HoudiniAssetInstanceInputField->GetComboButton();
	if(AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnStaticMeshDropped(Object, HoudiniAssetInstanceInputField, Idx, VariationIdx);
	}
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationRoll(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
											int32 VariationIdx) const
{
	const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	return Rotator.Roll;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationPitch(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
											 int32 VariationIdx) const
{
	const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	return Rotator.Pitch;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationYaw(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
										   int32 VariationIdx) const
{
	const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	return Rotator.Yaw;
}


void
UHoudiniAssetInstanceInput::SetRotationRoll(float Value, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	Rotator.Roll = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetRotationPitch(float Value, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	Rotator.Pitch = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetRotationYaw(float Value, 
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	Rotator.Yaw = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleX(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx) const
{
	const FVector& Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	return Scale3D.X;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleY(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx) const
{
	const FVector& Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	return Scale3D.Y;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleZ(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx) const
{
	const FVector& Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	return Scale3D.Z;
}


void
UHoudiniAssetInstanceInput::SetScaleX(float Value, 
									  UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	Scale3D.X = Value;

	if (HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
	{
		Scale3D.Y = Value;
		Scale3D.Z = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetScaleY(float Value, 
									  UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME), 
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	Scale3D.Y = Value;

	if (HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
	{
		Scale3D.X = Value;
		Scale3D.Z = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::SetScaleZ(float Value, 
									  UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	Scale3D.Z = Value;

	if (HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
	{
		Scale3D.Y = Value;
		Scale3D.X = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInput::CheckStateChanged(ECheckBoxState NewState,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	HoudiniAssetInstanceInputField->SetLinearOffsetScale(ECheckBoxState::Checked == NewState, VariationIdx);
}


ECheckBoxState
UHoudiniAssetInstanceInput::IsChecked(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
									  int32 VariationIdx) const
{
	if (HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
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

