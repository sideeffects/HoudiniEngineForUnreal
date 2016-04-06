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
#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniEngineString.h"


UHoudiniAssetInstanceInput::UHoudiniAssetInstanceInput(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ObjectToInstanceId(-1),
	bIsAttributeInstancer(false),
	bAttributeInstancerOverride(false)
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

	std::string MarshallingAttributeInstanceOverride = HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE;
	UHoudiniRuntimeSettings::GetSettingsValue(TEXT("MarshallingAttributeInstanceOverride"),
		MarshallingAttributeInstanceOverride);

	// Get object to be instanced.
	HAPI_ObjectId ObjectToInstance = InHoudiniGeoPartObject.HapiObjectGetToInstanceId();

	// If this is an attribute instancer, see if attribute exists.
	bool bAttributeCheck = InHoudiniGeoPartObject.HapiCheckAttributeExistance(HAPI_UNREAL_ATTRIB_INSTANCE,
		HAPI_ATTROWNER_POINT);

	// Check if this is an attribute override instancer (on detail or point).
	bool bAttributeOverrideCheck =
		InHoudiniGeoPartObject.HapiCheckAttributeExistance(MarshallingAttributeInstanceOverride,
			HAPI_ATTROWNER_DETAIL);

	bAttributeOverrideCheck |=
		InHoudiniGeoPartObject.HapiCheckAttributeExistance(MarshallingAttributeInstanceOverride,
			HAPI_ATTROWNER_POINT);

	// This is invalid combination, no object to instance and input is not an attribute instancer.
	if(!bAttributeCheck && !bAttributeOverrideCheck && -1 == ObjectToInstance)
	{
		return HoudiniAssetInstanceInput;
	}

	HoudiniAssetInstanceInput = NewObject<UHoudiniAssetInstanceInput>(InHoudiniAssetComponent, 
		UHoudiniAssetInstanceInput::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->HoudiniGeoPartObject = InHoudiniGeoPartObject;
	HoudiniAssetInstanceInput->SetNameAndLabel(InHoudiniGeoPartObject.ObjectName);
	HoudiniAssetInstanceInput->ObjectToInstanceId = ObjectToInstance;

	// Check if this instancer is an attribute instancer and if it is, mark it as such.
	HoudiniAssetInstanceInput->bIsAttributeInstancer = bAttributeCheck;

	// Check if this instancer is an attribute override instancer.
	HoudiniAssetInstanceInput->bAttributeInstancerOverride = bAttributeOverrideCheck;

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
	HoudiniAssetInstanceInput->bAttributeInstancerOverride = InstanceInput->bAttributeInstancerOverride;

	return HoudiniAssetInstanceInput;
}


bool
UHoudiniAssetInstanceInput::CreateInstanceInput()
{
	if(!HoudiniAssetComponent)
	{
		return false;
	}

	HAPI_AssetId AssetId = HoudiniAssetComponent->GetAssetId();

	// Retrieve instance transforms (for each point).
	TArray<FTransform> AllTransforms;
	HoudiniGeoPartObject.HapiGetInstanceTransforms(AssetId, AllTransforms);

	// List of new fields. Reused input fields will also be placed here.
	TArray<UHoudiniAssetInstanceInputField*> NewInstanceInputFields;

	if(bIsAttributeInstancer)
	{
		HAPI_AttributeInfo ResultAttributeInfo;
		TArray<FString> PointInstanceValues;

		if(!HoudiniGeoPartObject.HapiGetAttributeDataAsString(AssetId, HAPI_UNREAL_ATTRIB_INSTANCE,
			HAPI_ATTROWNER_POINT, ResultAttributeInfo, PointInstanceValues))
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
				const FHoudiniGeoPartObject& ItemHoudiniGeoPartObject =
					InstanceGeoPartObjects[InstanceGeoPartObjectIdx];

				// Locate or create an input field.
				CreateInstanceInputField(ItemHoudiniGeoPartObject, ObjectTransforms, InstancePath, InstanceInputFields,
					NewInstanceInputFields);
			}
		}
	}
	else if(bAttributeInstancerOverride)
	{
		// This is an attribute override. Unreal mesh is specified through an attribute and we use points.

		std::string MarshallingAttributeInstanceOverride = HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE;
		UHoudiniRuntimeSettings::GetSettingsValue(TEXT("MarshallingAttributeInstanceOverride"),
			MarshallingAttributeInstanceOverride);

		HAPI_AttributeInfo ResultAttributeInfo;
		if(!HoudiniGeoPartObject.HapiGetAttributeInfo(AssetId, MarshallingAttributeInstanceOverride,
			ResultAttributeInfo))
		{
			// We had an error while retrieving the attribute info.
			return false;
		}

		if(!ResultAttributeInfo.exists)
		{
			// Attribute does not exist.
			return false;
		}

		if(HAPI_ATTROWNER_DETAIL == ResultAttributeInfo.owner)
		{
			// Attribute is on detail, this means it gets applied to all points.

			TArray<FString> DetailInstanceValues;

			if(!HoudiniGeoPartObject.HapiGetAttributeDataAsString(AssetId, MarshallingAttributeInstanceOverride,
				HAPI_ATTROWNER_DETAIL, ResultAttributeInfo, DetailInstanceValues))
			{
				// This should not happen - attribute exists, but there was an error retrieving it.
				return false;
			}

			if(0 == DetailInstanceValues.Num())
			{
				// No values specified.
				return false;
			}

			// Attempt to load specified mesh.
			const FString& StaticMeshName = DetailInstanceValues[0];
			UStaticMesh* AttributeStaticMesh =
				Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *StaticMeshName, nullptr,
					LOAD_NoWarn, nullptr));

			if(AttributeStaticMesh)
			{
				CreateInstanceInputField(AttributeStaticMesh, AllTransforms, InstanceInputFields,
					NewInstanceInputFields);
			}
			else
			{
				return false;
			}
		}
		else if(HAPI_ATTROWNER_POINT == ResultAttributeInfo.owner)
		{
			TArray<FString> PointInstanceValues;

			if(!HoudiniGeoPartObject.HapiGetAttributeDataAsString(AssetId, MarshallingAttributeInstanceOverride,
				HAPI_ATTROWNER_POINT, ResultAttributeInfo, PointInstanceValues))
			{
				// This should not happen - attribute exists, but there was an error retrieving it.
				return false;
			}

			// Attribute is on points, number of points must match number of transforms.
			if(PointInstanceValues.Num() != AllTransforms.Num())
			{
				// This should not happen, we have mismatch between number of instance values and transforms.
				return false;
			}

			// Get unique names.
			TSet<FString> UniquePointInstanceValues(PointInstanceValues);

			// If instance attribute exists on points, we need to get all unique values.
			TMap<FString, UStaticMesh*> ObjectsToInstance;

			for(TSet<FString>::TIterator IterString(UniquePointInstanceValues); IterString; ++IterString)
			{
				const FString& UniqueName = *IterString;

				if(!ObjectsToInstance.Contains(UniqueName))
				{
					UStaticMesh* AttributeStaticMesh =
						Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *UniqueName,
							nullptr, LOAD_NoWarn, nullptr));
					ObjectsToInstance.Add(UniqueName, AttributeStaticMesh);
				}
			}

			if(0 == ObjectsToInstance.Num())
			{
				// We have no objects to instance.
				return false;
			}

			for(TMap<FString, UStaticMesh*>::TIterator IterInstanceObjects(ObjectsToInstance);
				IterInstanceObjects; ++IterInstanceObjects)
			{
				const FString& InstancePath = IterInstanceObjects.Key();
				UStaticMesh* AttributeStaticMesh = IterInstanceObjects.Value();

				if(AttributeStaticMesh)
				{
					TArray<FTransform> ObjectTransforms;
					GetPathInstaceTransforms(InstancePath, PointInstanceValues, AllTransforms, ObjectTransforms);

					CreateInstanceInputField(AttributeStaticMesh, ObjectTransforms, InstanceInputFields,
						NewInstanceInputFields);
				}
			}
		}
		else
		{
			// We don't support this attribute on other owners.
			return false;
		}
	}
	else
	{
		// This is a standard object type instancer.

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
	CleanInstanceInputFields(InstanceInputFields);
	InstanceInputFields = NewInstanceInputFields;

	return true;
}


UHoudiniAssetInstanceInputField*
UHoudiniAssetInstanceInput::LocateInputField(const FHoudiniGeoPartObject& GeoPartObject,
	const FString& InstancePathName)
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
UHoudiniAssetInstanceInput::LocateInputFieldsWithOriginalStaticMesh(TArray<UHoudiniAssetInstanceInputField*>& Fields,
	UStaticMesh* OriginalStaticMesh)
{
	Fields.Empty();

	UHoudiniAssetInstanceInputField* FoundHoudiniAssetInstanceInputField = nullptr;
	for(int32 FieldIdx = 0; FieldIdx < InstanceInputFields.Num(); ++FieldIdx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[FieldIdx];
		if(HoudiniAssetInstanceInputField && HoudiniAssetInstanceInputField->OriginalStaticMesh == OriginalStaticMesh)
		{
			Fields.Add(HoudiniAssetInstanceInputField);
		}
	}
}


void
UHoudiniAssetInstanceInput::CleanInstanceInputFields(TArray<UHoudiniAssetInstanceInputField*>& InInstanceInputFields)
{
	UHoudiniAssetInstanceInputField* FoundHoudiniAssetInstanceInputField = nullptr;
	for(int32 FieldIdx = 0; FieldIdx < InstanceInputFields.Num(); ++FieldIdx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[FieldIdx];
		if(HoudiniAssetInstanceInputField)
		{
			HoudiniAssetInstanceInputField->ConditionalBeginDestroy();
		}
	}

	InInstanceInputFields.Empty();
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
		HoudiniAssetInstanceInputField = UHoudiniAssetInstanceInputField::Create(HoudiniAssetComponent, this,
			InHoudiniGeoPartObject, InstancePathName);

		// Assign original and static mesh.
		HoudiniAssetInstanceInputField->OriginalStaticMesh = StaticMesh;
		HoudiniAssetInstanceInputField->AddInstanceVariation(StaticMesh, 0);

	}
	else
	{
		// Remove item from old list.
		InstanceInputFields.RemoveSingleSwap(HoudiniAssetInstanceInputField, false);

		TArray<int> MatchingIndices;

		HoudiniAssetInstanceInputField->FindStaticMeshIndices(HoudiniAssetInstanceInputField->OriginalStaticMesh,
			MatchingIndices);

		for(int Idx = 0; Idx < MatchingIndices.Num(); Idx++)
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
UHoudiniAssetInstanceInput::CreateInstanceInputField(UStaticMesh* StaticMesh,
	const TArray<FTransform>& ObjectTransforms, const TArray<UHoudiniAssetInstanceInputField*>& OldInstanceInputFields,
	TArray<UHoudiniAssetInstanceInputField*>& NewInstanceInputFields)
{
	// Locate all fields which have this static mesh set as original mesh.
	TArray<UHoudiniAssetInstanceInputField*> CandidateFields;
	LocateInputFieldsWithOriginalStaticMesh(CandidateFields, StaticMesh);

	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = nullptr;

	if(CandidateFields.Num() > 0)
	{
		HoudiniAssetInstanceInputField = CandidateFields[0];
		InstanceInputFields.RemoveSingleSwap(HoudiniAssetInstanceInputField, false);

		TArray<int> MatchingIndices;

		HoudiniAssetInstanceInputField->FindStaticMeshIndices(HoudiniAssetInstanceInputField->OriginalStaticMesh,
			MatchingIndices);

		for(int Idx = 0; Idx < MatchingIndices.Num(); Idx++)
		{
			int ReplacementIndex = MatchingIndices[Idx];
			HoudiniAssetInstanceInputField->ReplaceInstanceVariation(StaticMesh, ReplacementIndex);
		}

		HoudiniAssetInstanceInputField->OriginalStaticMesh = StaticMesh;
	}
	else
	{
		FHoudiniGeoPartObject TempHoudiniGeoPartObject;
		HoudiniAssetInstanceInputField = UHoudiniAssetInstanceInputField::Create(HoudiniAssetComponent, this,
			TempHoudiniGeoPartObject, TEXT(""));

		// Assign original and static mesh.
		HoudiniAssetInstanceInputField->OriginalStaticMesh = StaticMesh;
		HoudiniAssetInstanceInputField->AddInstanceVariation(StaticMesh, 0);
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


void
UHoudiniAssetInstanceInput::OnAddInstanceVariation(UHoudiniAssetInstanceInputField* InstanceInputField, int32 Index)
{
	UStaticMesh * StaticMesh =  InstanceInputField->GetInstanceVariation(Index);
	InstanceInputField->AddInstanceVariation( StaticMesh, Index );

	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}
}


void
UHoudiniAssetInstanceInput::OnRemoveInstanceVariation(UHoudiniAssetInstanceInputField* InstanceInputField, int32 Index)
{
	InstanceInputField->RemoveInstanceVariation(Index);

	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}
}


void
UHoudiniAssetInstanceInput::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// Classes allowed by instanced inputs.
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UStaticMesh::StaticClass());

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];

		for(int32 VariationIdx = 0;
			VariationIdx < HoudiniAssetInstanceInputField->InstanceVariationCount(); VariationIdx++)
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
			TSharedPtr<SHorizontalBox> HorizontalBox = nullptr;
			TSharedPtr<SBorder> StaticMeshThumbnailBorder;

			VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
			[
				SNew(SAssetDropTarget)
				.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this,
					&UHoudiniAssetInstanceInput::OnStaticMeshDraggedOver))
				.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this,
					&UHoudiniAssetInstanceInput::OnStaticMeshDropped, HoudiniAssetInstanceInputField, Idx,
						VariationIdx))
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
					&UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder, HoudiniAssetInstanceInputField, Idx,
						VariationIdx)))
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

			HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 28.0f, 0.0f, 28.0f)
			[
				PropertyCustomizationHelpers::MakeAddButton(
					FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInstanceInput::OnAddInstanceVariation,
						HoudiniAssetInstanceInputField, VariationIdx),
						LOCTEXT("AddAnotherInstanceToolTip", "Add Another Instance"))
			];

			HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 28.0f, 4.0f, 28.0f)
			[
				PropertyCustomizationHelpers::MakeRemoveButton(
					FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInstanceInput::OnRemoveInstanceVariation,
						HoudiniAssetInstanceInputField, VariationIdx),
						LOCTEXT("RemoveLastInstanceToolTip", "Remove Last Instance"))
			];

			// Store thumbnail border for this static mesh.
			HoudiniAssetInstanceInputField->AssignThumbnailBorder(StaticMeshThumbnailBorder);

			TSharedPtr<SComboButton> AssetComboButton;
			TSharedPtr<SHorizontalBox> ButtonBox;

			HorizontalBox->AddSlot()
				.FillWidth(10.0f)
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
							.OnMenuOpenChanged(FOnIsOpenChanged::CreateUObject(this,
								&UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton,
									HoudiniAssetInstanceInputField, Idx, VariationIdx))
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

			// Create asset picker for this combo button.
			{
				TArray<UFactory*> NewAssetFactories;
				TSharedRef<SWidget> PropertyMenuAssetPicker =
					PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(StaticMesh), true,
					AllowedClasses, NewAssetFactories, OnShouldFilterStaticMesh,
					FOnAssetSelected::CreateUObject(this, &UHoudiniAssetInstanceInput::OnStaticMeshSelected,
						HoudiniAssetInstanceInputField, Idx, VariationIdx),
					FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInstanceInput::CloseStaticMeshComboButton,
						HoudiniAssetInstanceInputField, Idx, VariationIdx));

				AssetComboButton->SetMenuContent(PropertyMenuAssetPicker);
			}

			// Store combo button for this static mesh.
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
							&UHoudiniAssetInstanceInput::GetRotationRoll, HoudiniAssetInstanceInputField,
								VariationIdx)))
					.Pitch(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
							&UHoudiniAssetInstanceInput::GetRotationPitch, HoudiniAssetInstanceInputField,
								VariationIdx)))
					.Yaw(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
							&UHoudiniAssetInstanceInput::GetRotationYaw, HoudiniAssetInstanceInputField,
								VariationIdx)))
					.OnRollChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetRotationRoll, HoudiniAssetInstanceInputField,
							VariationIdx))
					.OnPitchChanged(FOnFloatValueChanged::CreateUObject(this,
						&UHoudiniAssetInstanceInput::SetRotationPitch, HoudiniAssetInstanceInputField,
							VariationIdx))
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
					.X(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
							&UHoudiniAssetInstanceInput::GetScaleX, HoudiniAssetInstanceInputField, VariationIdx)))
					.Y(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
							&UHoudiniAssetInstanceInput::GetScaleY, HoudiniAssetInstanceInputField, VariationIdx)))
					.Z(TAttribute<TOptional<float> >::Create(
						TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
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
		InstanceInputFields[Idx]->HoudiniAssetInstanceInput = this;
	}
}


void
UHoudiniAssetInstanceInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << HoudiniAssetInstanceInputFlagsPacked;
	Ar << HoudiniGeoPartObject;

	Ar << ObjectToInstanceId;

	// Serialize fields.
	Ar << InstanceInputFields;
}


void
UHoudiniAssetInstanceInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Cast<UHoudiniAssetInstanceInput>(InThis);
	if(HoudiniAssetInstanceInput)
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


bool
UHoudiniAssetInstanceInput::IsAttributeInstancerOverride() const
{
	return bAttributeInstancerOverride;
}


#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::CloneComponentsAndAttachToActor(AActor* Actor)
{
	USceneComponent* RootComponent = Actor->GetRootComponent();

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];

		bool HasBakedOriginalStaticMesh = false;

		for (int32 VariationIdx = 0;
			VariationIdx < HoudiniAssetInstanceInputField->InstanceVariationCount(); VariationIdx++)
		{
			UStaticMesh* OutStaticMesh = nullptr;

			UInstancedStaticMeshComponent* InstancedStaticMeshComponent =
				HoudiniAssetInstanceInputField->GetInstancedStaticMeshComponent(VariationIdx);

			// If original static mesh is used, then we need to bake it.
			if (HoudiniAssetInstanceInputField->IsOriginalStaticMeshUsed(VariationIdx) && !HasBakedOriginalStaticMesh)
			{

				const FHoudiniGeoPartObject& ItemHoudiniGeoPartObject =
					HoudiniAssetComponent->LocateGeoPartObject(
						HoudiniAssetInstanceInputField->GetInstanceVariation(VariationIdx));

				// Bake the referenced static mesh.
				OutStaticMesh =
					FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(
						HoudiniAssetInstanceInputField->GetOriginalStaticMesh(),
							HoudiniAssetComponent, ItemHoudiniGeoPartObject, true);

				HasBakedOriginalStaticMesh = true;
				if (OutStaticMesh)
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
				OutStaticMesh = HoudiniAssetInstanceInputField->GetInstanceVariation(VariationIdx);
			}

			UInstancedStaticMeshComponent* DuplicatedComponent =
				NewObject<UInstancedStaticMeshComponent>(Actor, UInstancedStaticMeshComponent::StaticClass(), NAME_None,
					RF_Public);

			Actor->AddInstanceComponent(DuplicatedComponent);
			DuplicatedComponent->SetStaticMesh(OutStaticMesh);

			// Set component instances.
			{
				FRotator RotationOffset = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
				FVector ScaleOffset = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);

				const TArray<FTransform>& InstancedTransforms =
					HoudiniAssetInstanceInputField->GetInstancedTransforms(VariationIdx);

				FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(DuplicatedComponent,
					InstancedTransforms, RotationOffset, ScaleOffset);
			}

			// Copy visibility.
			DuplicatedComponent->SetVisibility(InstancedStaticMeshComponent->IsVisible());

			DuplicatedComponent->AttachTo(RootComponent);
			DuplicatedComponent->RegisterComponent();
			DuplicatedComponent->GetBodyInstance()->bAutoWeld = false;

		}
		
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


#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::OnStaticMeshDropped(UObject* InObject,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx, int32 VariationIdx)
{
	UStaticMesh* InputStaticMesh = Cast<UStaticMesh>(InObject);
	UStaticMesh* UsedStaticMesh = HoudiniAssetInstanceInputField->GetInstanceVariation(VariationIdx);

	if(InputStaticMesh && UsedStaticMesh != InputStaticMesh)
	{
		FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
		HoudiniAssetInstanceInputField->Modify();

		HoudiniAssetInstanceInputField->ReplaceInstanceVariation(InputStaticMesh, VariationIdx);

		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->UpdateEditorProperties(false);
		}
	}
}


const FSlateBrush*
UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder(
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx, int32 VariationIdx) const
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
UHoudiniAssetInstanceInput::OnThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent,
	UObject* Object)
{
	if(Object && GEditor)
	{
		GEditor->EditObject(Object);
	}

	return FReply::Handled();
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
	int32 Idx, int32 VariationIdx)
{
	UStaticMesh* OriginalStaticMesh = HoudiniAssetInstanceInputField->GetOriginalStaticMesh();
	OnStaticMeshDropped(OriginalStaticMesh, HoudiniAssetInstanceInputField, Idx, VariationIdx);

	return FReply::Handled();
}


void
UHoudiniAssetInstanceInput::CloseStaticMeshComboButton(UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 Idx, int32 VariationIdx)
{
	// Do nothing.
}


void
UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton(bool bOpened,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx, int32 VariationIdx)
{
	if(!bOpened)
	{
		// If combo button has been closed, update the UI.
		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->UpdateEditorProperties(false);
		}
	}
}


void
UHoudiniAssetInstanceInput::OnStaticMeshSelected(const FAssetData& AssetData,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 Idx, int32 VariationIdx)
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
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	Rotator.Roll = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms(false);
}


void
UHoudiniAssetInstanceInput::SetRotationPitch(float Value,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	Rotator.Pitch = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms(false);
}


void
UHoudiniAssetInstanceInput::SetRotationYaw(float Value,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset(VariationIdx);
	Rotator.Yaw = Value;
	HoudiniAssetInstanceInputField->SetRotationOffset(Rotator, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms(false);
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
UHoudiniAssetInstanceInput::SetScaleX(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	Scale3D.X = Value;

	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
	{
		Scale3D.Y = Value;
		Scale3D.Z = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms(false);
}


void
UHoudiniAssetInstanceInput::SetScaleY(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME), 
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	Scale3D.Y = Value;

	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
	{
		Scale3D.X = Value;
		Scale3D.Z = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms(false);
}


void
UHoudiniAssetInstanceInput::SetScaleZ(float Value, UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField,
	int32 VariationIdx)
{
	FScopedTransaction Transaction(TEXT(HOUDINI_MODULE_RUNTIME),
		LOCTEXT("HoudiniInstanceInputChange", "Houdini Instance Input Change"), HoudiniAssetComponent);
	HoudiniAssetInstanceInputField->Modify();

	FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset(VariationIdx);
	Scale3D.Z = Value;

	if(HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly(VariationIdx))
	{
		Scale3D.Y = Value;
		Scale3D.X = Value;
	}

	HoudiniAssetInstanceInputField->SetScaleOffset(Scale3D, VariationIdx);
	HoudiniAssetInstanceInputField->UpdateInstanceTransforms(false);
}


void
UHoudiniAssetInstanceInput::CheckStateChanged(ECheckBoxState NewState,
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx)
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


bool
UHoudiniAssetInstanceInput::CollectAllInstancedStaticMeshComponents(TArray<UInstancedStaticMeshComponent*>& Components,
	UStaticMesh* StaticMesh)
{
	bool bCollected = false;

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		if(HoudiniAssetInstanceInputField)
		{
			UStaticMesh* OriginalStaticMesh = HoudiniAssetInstanceInputField->GetOriginalStaticMesh();
			if(OriginalStaticMesh == StaticMesh)
			{
				for(int32 IdxMesh = 0; IdxMesh < HoudiniAssetInstanceInputField->StaticMeshes.Num(); ++IdxMesh)
				{
					UStaticMesh* UsedStaticMesh = HoudiniAssetInstanceInputField->StaticMeshes[IdxMesh];
					if(UsedStaticMesh == StaticMesh)
					{
						Components.Add(HoudiniAssetInstanceInputField->InstancedStaticMeshComponents[IdxMesh]);
						bCollected = true;
					}
				}
			}
		}
	}

	return bCollected;
}


bool
UHoudiniAssetInstanceInput::GetMaterialReplacementMeshes(UMaterialInterface* Material,
	TMap<UStaticMesh*, int32>& MaterialReplacementsMap)
{
	bool bResult = false;

	for(int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx)
	{
		UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = InstanceInputFields[Idx];
		if(HoudiniAssetInstanceInputField)
		{
			bResult |= HoudiniAssetInstanceInputField->GetMaterialReplacementMeshes(Material, MaterialReplacementsMap);
		}
	}

	return bResult;
}

