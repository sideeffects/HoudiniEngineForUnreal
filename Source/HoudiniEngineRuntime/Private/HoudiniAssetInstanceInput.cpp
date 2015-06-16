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
#include "HoudiniApi.h"


const float
UHoudiniAssetInstanceInput::ScaleSmallValue = KINDA_SMALL_NUMBER * 2.0f;


UHoudiniAssetInstanceInput::UHoudiniAssetInstanceInput(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ObjectId(-1),
	ObjectToInstanceId(-1),
	GeoId(-1),
	PartId(-1),
	bIsAttributeInstancer(false)
{
	TupleSize = 0;
}


UHoudiniAssetInstanceInput::~UHoudiniAssetInstanceInput()
{

}


UHoudiniAssetInstanceInput*
UHoudiniAssetInstanceInput::Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
	const FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

	// Get name of this input. For the time being we only support geometry inputs.
	HAPI_ObjectInfo ObjectInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetObjects(nullptr, InHoudiniAssetComponent->GetAssetId(), &ObjectInfo,
		HoudiniGeoPartObject.ObjectId, 1))
	{
		return HoudiniAssetInstanceInput;
	}

	// If this is an attribute instancer, see if attribute exists.
	bool bAttributeCheck = UHoudiniAssetInstanceInput::CheckInstanceAttribute(InHoudiniAssetComponent->GetAssetId(),
		HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId);

	// This is invalid combination, no object to instance and input is not an attribute instancer.
	if(!bAttributeCheck && -1 == ObjectInfo.objectToInstanceId)
	{
		return HoudiniAssetInstanceInput;
	}

	HoudiniAssetInstanceInput = NewObject<UHoudiniAssetInstanceInput>(InHoudiniAssetComponent);

	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->SetNameAndLabel(ObjectInfo.nameSH);
	HoudiniAssetInstanceInput->SetObjectGeoPartIds(HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId,
		HoudiniGeoPartObject.PartId);
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
	FHoudiniEngineUtils::HapiGetInstanceTransforms(HoudiniAssetComponent->GetAssetId(), ObjectId, GeoId, PartId,
		AllTransforms);

	// Store old tuple size.
	int32 OldTupleSize = TupleSize;

	if(bIsAttributeInstancer)
	{
		HAPI_AttributeInfo ResultAttributeInfo;
		TArray<FString> PointInstanceValues;

		if(!FHoudiniEngineUtils::HapiGetAttributeDataAsString(HoudiniAssetComponent->GetAssetId(), ObjectId, GeoId,
			PartId, HAPI_UNREAL_ATTRIB_INSTANCE, ResultAttributeInfo, PointInstanceValues))
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

		// If instance attribute exists on points, we need to get all unique values.
		TMultiMap<FString, FHoudiniGeoPartObject> ObjectsToInstance;
		TSet<FString> UniquePointInstanceValues(PointInstanceValues);
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

		// Adjust number of resources according to number of objects we need to instance.
		TupleSize = ObjectsToInstance.Num();
		check(TupleSize);
		AdjustMeshComponentResources(TupleSize, OldTupleSize);

		// Process each existing detected instancer and create new ones if necessary.
		int32 GeoIdx = 0;
		for(TMultiMap<FString, FHoudiniGeoPartObject>::TIterator
			IterInstancer(ObjectsToInstance); IterInstancer; ++IterInstancer)
		{
			const FString& ObjectInstancePath = IterInstancer.Key();
			const FHoudiniGeoPartObject& HoudiniGeoPartObject = IterInstancer.Value();

			// Set component transformation.
			InstancedStaticMeshComponents[GeoIdx]->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

			if(!OriginalStaticMeshes[GeoIdx])
			{
				// Locate static mesh for this geo part.
				UStaticMesh* StaticMesh = HoudiniAssetComponent->LocateStaticMesh(HoudiniGeoPartObject);
				check(StaticMesh);

				OriginalStaticMeshes[GeoIdx] = StaticMesh;
			}

			// If static mesh is not set, assign it.
			if(!StaticMeshes[GeoIdx])
			{
				StaticMeshes[GeoIdx] = OriginalStaticMeshes[GeoIdx];
				InstancedStaticMeshComponents[GeoIdx]->SetStaticMesh(StaticMeshes[GeoIdx]);
			}

			// Retrieve all applicable transforms for this object.
			TArray<FTransform> ObjectTransforms;
			GetPathInstaceTransforms(ObjectInstancePath, PointInstanceValues, AllTransforms, ObjectTransforms);
			check(ObjectTransforms.Num());

			// Store transforms for this component.
			InstancedTransforms[GeoIdx].Append(ObjectTransforms);

			// Set component's transformations and instances.
			SetComponentInstanceTransformations(InstancedStaticMeshComponents[GeoIdx], ObjectTransforms, GeoIdx);

			// If this is a collision instancer, make it invisible.
			if(HoudiniGeoPartObject.bIsCollidable)
			{
				InstancedStaticMeshComponents[GeoIdx]->SetVisibility(false);
			}

			++GeoIdx;
		}
	}
	else
	{
		// Locate all geo objects requiring instancing (can be multiple if geo / part / object split took place).
		TArray<FHoudiniGeoPartObject> ObjectsToInstance;
		HoudiniAssetComponent->LocateStaticMeshes(ObjectToInstanceId, ObjectsToInstance);

		// Adjust number of resources according to number of objects we need to instance.
		TupleSize = ObjectsToInstance.Num();
		AdjustMeshComponentResources(TupleSize, OldTupleSize);

		// Process each existing detected object that needs to be instanced.
		for(int32 GeoIdx = 0; GeoIdx < ObjectsToInstance.Num(); ++GeoIdx)
		{
			const FHoudiniGeoPartObject& HoudiniGeoPartObject = ObjectsToInstance[GeoIdx];

			// Set component transformation.
			InstancedStaticMeshComponents[GeoIdx]->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

			if(!OriginalStaticMeshes[GeoIdx])
			{
				// Locate static mesh corresponding to this geo part object.
				UStaticMesh* StaticMesh = HoudiniAssetComponent->LocateStaticMesh(HoudiniGeoPartObject);
				check(StaticMesh);

				OriginalStaticMeshes[GeoIdx] = StaticMesh;
			}

			// If static mesh is not set, assign it.
			if(!StaticMeshes[GeoIdx])
			{
				StaticMeshes[GeoIdx] = OriginalStaticMeshes[GeoIdx];
				InstancedStaticMeshComponents[GeoIdx]->SetStaticMesh(StaticMeshes[GeoIdx]);
			}

			// Store transforms for this component.
			InstancedTransforms[GeoIdx].Append(AllTransforms);

			// Set component's transformations and instances.
			SetComponentInstanceTransformations(InstancedStaticMeshComponents[GeoIdx], AllTransforms, GeoIdx);

			// If this is a collision instancer, make it invisible.
			if(HoudiniGeoPartObject.bIsCollidable)
			{
				InstancedStaticMeshComponents[GeoIdx]->SetVisibility(false);
			}
		}
	}

	return true;
}


bool
UHoudiniAssetInstanceInput::CreateInstanceInputPostLoad()
{
	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		// Get geo part information for this index.
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = GeoPartObjects[Idx];

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

	GeoPartObjects.Empty();
	return true;
}


void
UHoudiniAssetInstanceInput::RecreateRenderStates()
{
	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent = InstancedStaticMeshComponents[Idx];
		if(InstancedStaticMeshComponent)
		{
			InstancedStaticMeshComponent->RecreateRenderState_Concurrent();
		}
	}
}


void
UHoudiniAssetInstanceInput::RecreatePhysicsStates()
{
	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent = InstancedStaticMeshComponents[Idx];
		if(InstancedStaticMeshComponent)
		{
			InstancedStaticMeshComponent->RecreatePhysicsState();
		}
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

	StaticMeshThumbnailBorders.Empty();
	StaticMeshComboButtons.Empty();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	for(int32 StaticMeshIdx = 0; StaticMeshIdx < StaticMeshes.Num(); ++StaticMeshIdx)
	{
		UStaticMesh* StaticMesh = StaticMeshes[StaticMeshIdx];

		FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
		FText LabelText =
			FText::Format(LOCTEXT("HoudiniStaticMeshInstanceInput", "Static Mesh Instance {0}"),
				FText::AsNumber(StaticMeshIdx));

		Row.NameWidget.Widget = SNew(STextBlock)
								.Text(LabelText)
								.ToolTipText(LabelText)
								.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		// Create thumbnail for this mesh.
		TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(new FAssetThumbnail(StaticMesh, 64, 64,
			AssetThumbnailPool));
		TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
		TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
		TSharedPtr<SBorder>StaticMeshThumbnailBorder;

		VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
		[
			SNew(SAssetDropTarget)
			.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this,
				&UHoudiniAssetInstanceInput::OnStaticMeshDraggedOver))
			.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this,
				&UHoudiniAssetInstanceInput::OnStaticMeshDropped, StaticMesh, StaticMeshIdx))
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
					&UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder, StaticMesh, StaticMeshIdx)))
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
		StaticMeshThumbnailBorders.Add(StaticMeshIdx, StaticMeshThumbnailBorder);

		TSharedPtr<SComboButton> AssetComboButton;
		TSharedPtr<SHorizontalBox> ButtonBox;

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
						&UHoudiniAssetInstanceInput::OnGetStaticMeshMenuContent, StaticMesh, StaticMeshIdx))
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
				StaticMesh, StaticMeshIdx))
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
			+SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			[
				SNew(SRotatorInputBox)
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.Roll(TAttribute<TOptional<float> >::Create(
					TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationRoll, StaticMeshIdx)))
				.Pitch(TAttribute<TOptional<float> >::Create(
					TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationPitch, StaticMeshIdx)))
				.Yaw(TAttribute<TOptional<float> >::Create(
					TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
						&UHoudiniAssetInstanceInput::GetRotationYaw, StaticMeshIdx)))
				.OnRollChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetRotationRoll, StaticMeshIdx))
				.OnPitchChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetRotationPitch, StaticMeshIdx))
				.OnYawChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetRotationYaw, StaticMeshIdx))
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
					&UHoudiniAssetInstanceInput::GetScaleX, StaticMeshIdx)))
				.Y(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::GetScaleY, StaticMeshIdx)))
				.Z(TAttribute<TOptional<float> >::Create(TAttribute<TOptional<float> >::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::GetScaleZ, StaticMeshIdx)))
				.OnXChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetScaleX, StaticMeshIdx))
				.OnYChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetScaleY, StaticMeshIdx))
				.OnZChanged(FOnFloatValueChanged::CreateUObject(this,
					&UHoudiniAssetInstanceInput::SetScaleZ, StaticMeshIdx))
			]
		];

		FText LabelLinearScaleText = LOCTEXT("HoudiniScaleFieldsLinearly", "Scale all fields linearly");
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this,
				&UHoudiniAssetInstanceInput::CheckStateChanged, StaticMeshIdx))
			.IsChecked(TAttribute<ECheckBoxState>::Create(
				TAttribute<ECheckBoxState>::FGetter::CreateUObject(this,
					&UHoudiniAssetInstanceInput::IsChecked, StaticMeshIdx)))
			.Content()
			[
				SNew(STextBlock)
				.Text(LabelLinearScaleText)
				.ToolTipText(LabelLinearScaleText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		// Store combo button static mesh mapping.
		StaticMeshComboButtons.Add(StaticMeshIdx, AssetComboButton);

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
	AdjustMeshComponentResources(0, TupleSize);
	TupleSize = 0;

	Super::BeginDestroy();
}


void
UHoudiniAssetInstanceInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);

	Ar << ObjectId;
	Ar << ObjectToInstanceId;
	Ar << GeoId;
	Ar << PartId;

	Ar << HoudiniAssetInstanceInputFlagsPacked;

	if(Ar.IsLoading())
	{
		InstancedTransforms.SetNum(TupleSize);
		for(int32 Idx = 0; Idx < TupleSize; ++Idx)
		{
			InstancedTransforms[Idx].SetNum(0);
		}

		StaticMeshes.SetNumZeroed(TupleSize);
		OriginalStaticMeshes.SetNumZeroed(TupleSize);
		InstancedStaticMeshComponents.SetNumZeroed(TupleSize);

		// By default, we have no rotations.
		RotationOffsets.SetNumZeroed(TupleSize);

		// By default, we have identity scales.
		static const FVector ScaleIdentity(1.0f, 1.0f, 1.0f);
		ScaleOffsets.Init(ScaleIdentity, TupleSize);

		ScaleOffsetsLinearly.Init(true, TupleSize);

		GeoPartObjects.SetNumZeroed(TupleSize);
	}

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		bool bValidComponent = false;

		if(!Ar.IsSaving() && !Ar.IsLoading())
		{
			continue;
		}

		if(Ar.IsSaving())
		{
			bValidComponent = (InstancedStaticMeshComponents[Idx] != nullptr);
		}

		Ar << bValidComponent;

		if(bValidComponent)
		{
			// Serialize rotation offset.
			Ar << RotationOffsets[Idx];

			// Serialize scale offset.
			Ar << ScaleOffsets[Idx];

			// Serialize whether this offset is scaled linearly.
			Ar << ScaleOffsetsLinearly[Idx];

			// Serialize all transforms;
			Ar << InstancedTransforms[Idx];

			// Serialize whether original mesh is used.
			bool bOriginalMeshUsed = false;

			if(Ar.IsSaving())
			{
				bOriginalMeshUsed = (StaticMeshes[Idx] == OriginalStaticMeshes[Idx]);
			}

			Ar << bOriginalMeshUsed;

			// Serialize original mesh's geo part.
			FHoudiniGeoPartObject GeoPartObject;
			if(Ar.IsSaving())
			{
				GeoPartObject = HoudiniAssetComponent->LocateGeoPartObject(OriginalStaticMeshes[Idx]);
			}

			GeoPartObject.Serialize(Ar);

			if(Ar.IsLoading())
			{
				// Store geo parts, we can use this information in post load to get original meshes.
				GeoPartObjects[Idx] = GeoPartObject;
			}

			// If original mesh is not used, we need to serialize it.
			if(!bOriginalMeshUsed)
			{
				FString MeshPathName = TEXT("");

				// If original mesh is not used, we need to store path to used mesh.
				if(Ar.IsSaving())
				{
					MeshPathName = StaticMeshes[Idx]->GetPathName();
				}

				Ar << MeshPathName;

				if(Ar.IsLoading())
				{
					StaticMeshes[Idx] = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
						*MeshPathName, nullptr, LOAD_NoWarn, nullptr));
				}
			}
		}
		else
		{
			// This component is not valid.
			check(false);
		}
	}
}


void
UHoudiniAssetInstanceInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Cast<UHoudiniAssetInstanceInput>(InThis);
	if(HoudiniAssetInstanceInput && !HoudiniAssetInstanceInput->IsPendingKill())
	{
		UStaticMesh* StaticMesh = nullptr;
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent = nullptr;

		// Add references to all used objects.
		for(int32 Idx = 0; Idx < HoudiniAssetInstanceInput->GetTupleSize(); ++Idx)
		{
			StaticMesh = HoudiniAssetInstanceInput->StaticMeshes[Idx];
			if(StaticMesh)
			{
				Collector.AddReferencedObject(StaticMesh, InThis);
			}

			StaticMesh = HoudiniAssetInstanceInput->OriginalStaticMeshes[Idx];
			if(StaticMesh)
			{
				Collector.AddReferencedObject(StaticMesh, InThis);
			}

			InstancedStaticMeshComponent = HoudiniAssetInstanceInput->InstancedStaticMeshComponents[Idx];
			if(InstancedStaticMeshComponent)
			{
				Collector.AddReferencedObject(InstancedStaticMeshComponent, InThis);
			}
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


void
UHoudiniAssetInstanceInput::SetObjectGeoPartIds(HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId)
{
	ObjectId = InObjectId;
	GeoId = InGeoId;
	PartId = InPartId;
}


void
UHoudiniAssetInstanceInput::AdjustMeshComponentResources(int32 ObjectCount, int32 OldTupleSize)
{
	static const FVector ScaleIdentity(1.0f, 1.0f, 1.0f);

	if(ObjectCount < OldTupleSize)
	{
		// If we have more than supplied object count, we need to free those unused resources.

		for(int32 Idx = ObjectCount; Idx < InstancedStaticMeshComponents.Num(); ++Idx)
		{
			UInstancedStaticMeshComponent* InstancedStaticMeshComponent = InstancedStaticMeshComponents[Idx];
			if(InstancedStaticMeshComponent)
			{
				InstancedStaticMeshComponent->DetachFromParent();
				InstancedStaticMeshComponent->UnregisterComponent();
				InstancedStaticMeshComponent->DestroyComponent();

				InstancedStaticMeshComponents[Idx] = nullptr;
			}

			StaticMeshes[Idx] = nullptr;
			OriginalStaticMeshes[Idx] = nullptr;
		}

		InstancedStaticMeshComponents.SetNum(ObjectCount);
		StaticMeshes.SetNum(ObjectCount);
		OriginalStaticMeshes.SetNum(ObjectCount);
		RotationOffsets.SetNum(ObjectCount);
		ScaleOffsets.SetNum(ObjectCount);
		ScaleOffsetsLinearly.SetNum(ObjectCount);
	}
	else if(ObjectCount > OldTupleSize)
	{
		int32 OldComponentCount = InstancedStaticMeshComponents.Num();

		// If we have less than supplied object count, we need to create new resources.
		InstancedStaticMeshComponents.SetNumZeroed(ObjectCount);
		StaticMeshes.SetNumZeroed(ObjectCount);
		OriginalStaticMeshes.SetNumZeroed(ObjectCount);
		RotationOffsets.SetNumZeroed(ObjectCount);

		// We need to add identity scales for new components.
		ScaleOffsets.SetNum(OldTupleSize);
		ScaleOffsetsLinearly.SetNum(OldTupleSize);

		for(int32 ScaleIdx = OldTupleSize; ScaleIdx < ObjectCount; ++ScaleIdx)
		{
			ScaleOffsets.Add(ScaleIdentity);
			ScaleOffsetsLinearly.Add(true);
		}

		for(int32 Idx = OldComponentCount; Idx < ObjectCount; ++Idx)
		{
			// We need to create instanced component.
			UInstancedStaticMeshComponent* Component =
				NewObject<UInstancedStaticMeshComponent>(HoudiniAssetComponent->GetOwner(),
					UInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Transient);

			Component->AttachTo(HoudiniAssetComponent);
			Component->RegisterComponent();
			Component->GetBodyInstance()->bAutoWeld = false;

			// We want to make this invisible if it's a collision instancer.
			Component->SetVisibility(true);

			InstancedStaticMeshComponents[Idx] = Component;
			StaticMeshes[Idx] = nullptr;
			OriginalStaticMeshes[Idx] = nullptr;
		}
	}

	// Reset transforms array.
	InstancedTransforms.SetNum(ObjectCount);
	for(int32 Idx = 0; Idx < ObjectCount; ++Idx)
	{
		InstancedTransforms[Idx].SetNum(0);
	}
}


void
UHoudiniAssetInstanceInput::SetComponentInstanceTransformations(UInstancedStaticMeshComponent* InstancedStaticMeshComponent,
	const TArray<FTransform>& InstanceTransforms, int32 Idx)
{
	InstancedStaticMeshComponent->ClearInstances();

	// Get current rotation and scale for this index.
	FRotator Rotator = RotationOffsets[Idx];
	FVector Scale = ScaleOffsets[Idx];

	for(int32 InstanceIdx = 0; InstanceIdx < InstanceTransforms.Num(); ++InstanceIdx)
	{
		FTransform Transform = InstanceTransforms[InstanceIdx];

		// Compute new rotation and scale.
		FQuat TransformRotation = Transform.GetRotation() * Rotator.Quaternion();
		FVector TransformScale3D = Transform.GetScale3D() * Scale;

		// Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances.
		// Happens in blueprint as well.
		if(TransformScale3D.X < UHoudiniAssetInstanceInput::ScaleSmallValue)
		{
			TransformScale3D.X = UHoudiniAssetInstanceInput::ScaleSmallValue;
		}

		if(TransformScale3D.Y < UHoudiniAssetInstanceInput::ScaleSmallValue)
		{
			TransformScale3D.Y = UHoudiniAssetInstanceInput::ScaleSmallValue;
		}

		if(TransformScale3D.Z < UHoudiniAssetInstanceInput::ScaleSmallValue)
		{
			TransformScale3D.Z = UHoudiniAssetInstanceInput::ScaleSmallValue;
		}

		Transform.SetRotation(TransformRotation);
		Transform.SetScale3D(TransformScale3D);

		InstancedStaticMeshComponent->AddInstance(Transform);
	}
}


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


void
UHoudiniAssetInstanceInput::ChangeInstancedStaticMeshComponentMesh(int32 Idx)
{
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent = InstancedStaticMeshComponents[Idx];
	UStaticMesh* StaticMesh = StaticMeshes[Idx];
	UStaticMesh* OriginalStaticMesh = OriginalStaticMeshes[Idx];
	if(InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
	}
}


void
UHoudiniAssetInstanceInput::UpdateInstanceTransforms(int32 Idx)
{
	// Get current rotation and scale for this index.
	FRotator Rotator = RotationOffsets[Idx];
	FVector Scale = ScaleOffsets[Idx];

	// Get component for this index.
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent = InstancedStaticMeshComponents[Idx];
	InstancedStaticMeshComponent->ClearInstances();

	TArray<FTransform>& Transforms = InstancedTransforms[Idx];
	for(int32 InstanceIdx = 0; InstanceIdx < Transforms.Num(); ++InstanceIdx)
	{
		// Get transform for this instance.
		FTransform Transform = Transforms[InstanceIdx];

		// Compute new rotation and scale.
		FQuat TransformRotation = Transform.GetRotation() * Rotator.Quaternion();
		FVector TransformScale3D = Transform.GetScale3D() * Scale;

		// Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances.
		// Happens in blueprint as well.
		if(TransformScale3D.X < UHoudiniAssetInstanceInput::ScaleSmallValue)
		{
			TransformScale3D.X = UHoudiniAssetInstanceInput::ScaleSmallValue;
		}

		if(TransformScale3D.Y < UHoudiniAssetInstanceInput::ScaleSmallValue)
		{
			TransformScale3D.Y = UHoudiniAssetInstanceInput::ScaleSmallValue;
		}

		if(TransformScale3D.Z < UHoudiniAssetInstanceInput::ScaleSmallValue)
		{
			TransformScale3D.Z = UHoudiniAssetInstanceInput::ScaleSmallValue;
		}

		Transform.SetRotation(TransformRotation);
		Transform.SetScale3D(TransformScale3D);

		InstancedStaticMeshComponent->AddInstance(Transform);
	}
}


bool
UHoudiniAssetInstanceInput::CheckInstanceAttribute(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId,
	HAPI_GeoId InGeoId, HAPI_PartId InPartId)
{
	if(-1 == InAssetId || -1 == InObjectId || -1 == InGeoId || -1 == InPartId)
	{
		return false;
	}

	return FHoudiniEngineUtils::HapiCheckAttributeExists(InAssetId, InObjectId, InGeoId, InPartId,
		HAPI_UNREAL_ATTRIB_INSTANCE, HAPI_ATTROWNER_POINT);
}


#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::OnStaticMeshDropped(UObject* InObject, UStaticMesh* StaticMesh, int32 StaticMeshIdx)
{
	UStaticMesh* InputStaticMesh = Cast<UStaticMesh>(InObject);
	if(InputStaticMesh && StaticMesh != InputStaticMesh)
	{
		// Change used static mesh.
		StaticMeshes[StaticMeshIdx] = InputStaticMesh;

		// Change component's mesh.
		ChangeInstancedStaticMeshComponentMesh(StaticMeshIdx);
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


const FSlateBrush*
UHoudiniAssetInstanceInput::GetStaticMeshThumbnailBorder(UStaticMesh* StaticMesh, int32 Idx) const
{
	TSharedPtr<SBorder> ThumbnailBorder = StaticMeshThumbnailBorders[Idx];
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
UHoudiniAssetInstanceInput::OnGetStaticMeshMenuContent(UStaticMesh* StaticMesh, int32 StaticMeshIdx)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UStaticMesh::StaticClass());

	TArray<UFactory*> NewAssetFactories;

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(FAssetData(StaticMesh), true,
		AllowedClasses, NewAssetFactories,OnShouldFilterStaticMesh,
		FOnAssetSelected::CreateUObject(this, &UHoudiniAssetInstanceInput::OnStaticMeshSelected,
			StaticMesh, StaticMeshIdx),
		FSimpleDelegate::CreateUObject(this, &UHoudiniAssetInstanceInput::CloseStaticMeshComboButton));
}


void
UHoudiniAssetInstanceInput::OnStaticMeshSelected(const FAssetData& AssetData, UStaticMesh* StaticMesh,
	int32 StaticMeshIdx)
{
	TSharedPtr<SComboButton> AssetComboButton = StaticMeshComboButtons[StaticMeshIdx];
	if(AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject* Object = AssetData.GetAsset();
		OnStaticMeshDropped(Object, StaticMesh, StaticMeshIdx);
	}
}


void
UHoudiniAssetInstanceInput::CloseStaticMeshComboButton()
{

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
UHoudiniAssetInstanceInput::OnResetStaticMeshClicked(UStaticMesh* StaticMesh, int32 StaticMeshIdx)
{
	UStaticMesh* OriginalStaticMesh = OriginalStaticMeshes[StaticMeshIdx];
	OnStaticMeshDropped(OriginalStaticMesh, StaticMesh, StaticMeshIdx);

	return FReply::Handled();
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationRoll(int32 Idx) const
{
	check(Idx >= 0 && Idx < RotationOffsets.Num())
	const FRotator& Rotator = RotationOffsets[Idx];

	return Rotator.Roll;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationPitch(int32 Idx) const
{
	check(Idx >= 0 && Idx < RotationOffsets.Num())
	const FRotator& Rotator = RotationOffsets[Idx];

	return Rotator.Pitch;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetRotationYaw(int32 Idx) const
{
	check(Idx >= 0 && Idx < RotationOffsets.Num())
	const FRotator& Rotator = RotationOffsets[Idx];

	return Rotator.Yaw;
}


void
UHoudiniAssetInstanceInput::SetRotationRoll(float Value, int32 Idx)
{
	check(Idx >= 0 && Idx < RotationOffsets.Num())
	FRotator& Rotator = RotationOffsets[Idx];

	Rotator.Roll = Value;

	UpdateInstanceTransforms(Idx);
}


void
UHoudiniAssetInstanceInput::SetRotationPitch(float Value, int32 Idx)
{
	check(Idx >= 0 && Idx < RotationOffsets.Num())
	FRotator& Rotator = RotationOffsets[Idx];

	Rotator.Pitch = Value;

	UpdateInstanceTransforms(Idx);
}


void
UHoudiniAssetInstanceInput::SetRotationYaw(float Value, int32 Idx)
{
	check(Idx >= 0 && Idx < RotationOffsets.Num())
	FRotator& Rotator = RotationOffsets[Idx];

	Rotator.Yaw = Value;

	UpdateInstanceTransforms(Idx);
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleX(int32 Idx) const
{
	check(Idx >= 0 && Idx < ScaleOffsets.Num())
	const FVector& Scale3D = ScaleOffsets[Idx];

	return Scale3D.X;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleY(int32 Idx) const
{
	check(Idx >= 0 && Idx < ScaleOffsets.Num())
	const FVector& Scale3D = ScaleOffsets[Idx];

	return Scale3D.Y;
}


TOptional<float>
UHoudiniAssetInstanceInput::GetScaleZ(int32 Idx) const
{
	check(Idx >= 0 && Idx < ScaleOffsets.Num())
	const FVector& Scale3D = ScaleOffsets[Idx];

	return Scale3D.Z;
}


void
UHoudiniAssetInstanceInput::SetScaleX(float Value, int32 Idx)
{
	check(Idx >= 0 && Idx < ScaleOffsets.Num())
	FVector& Scale3D = ScaleOffsets[Idx];

	Scale3D.X = Value;

	if(ScaleOffsetsLinearly[Idx])
	{
		Scale3D.Y = Value;
		Scale3D.Z = Value;
	}

	UpdateInstanceTransforms(Idx);
}


void
UHoudiniAssetInstanceInput::SetScaleY(float Value, int32 Idx)
{
	check(Idx >= 0 && Idx < ScaleOffsets.Num())
	FVector& Scale3D = ScaleOffsets[Idx];

	Scale3D.Y = Value;

	if(ScaleOffsetsLinearly[Idx])
	{
		Scale3D.X = Value;
		Scale3D.Z = Value;
	}

	UpdateInstanceTransforms(Idx);
}


void
UHoudiniAssetInstanceInput::SetScaleZ(float Value, int32 Idx)
{
	check(Idx >= 0 && Idx < ScaleOffsets.Num())
	FVector& Scale3D = ScaleOffsets[Idx];

	Scale3D.Z = Value;

	if(ScaleOffsetsLinearly[Idx])
	{
		Scale3D.X = Value;
		Scale3D.Y = Value;
	}

	UpdateInstanceTransforms(Idx);
}


void
UHoudiniAssetInstanceInput::CheckStateChanged(ECheckBoxState NewState, int32 Idx)
{
	check(Idx >= 0 && Idx < ScaleOffsetsLinearly.Num())
	ScaleOffsetsLinearly[Idx] = (ECheckBoxState::Checked == NewState);
}


ECheckBoxState
UHoudiniAssetInstanceInput::IsChecked(int32 Idx) const
{
	check(Idx >= 0 && Idx < ScaleOffsetsLinearly.Num())

	if(ScaleOffsetsLinearly[Idx])
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
	for(int32 MeshIdx = 0; MeshIdx < StaticMeshes.Num(); ++MeshIdx)
	{
		UStaticMesh* StaticMesh = StaticMeshes[MeshIdx];

		if(StaticMesh == OtherStaticMesh)
		{
			UStaticMeshComponent* StaticMeshComponent = InstancedStaticMeshComponents[MeshIdx];
			StaticMeshComponent->SetMaterial(MaterialIdx, MaterialInterface);
		}
	}
}
