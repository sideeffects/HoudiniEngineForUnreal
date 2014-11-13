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


UHoudiniAssetInstanceInput::UHoudiniAssetInstanceInput(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	ObjectId(-1),
	ObjectToInstanceId(-1),
	GeoId(-1),
	PartId(-1),
	bAttributeInstancer(false)
{
	TupleSize = 0;
}


UHoudiniAssetInstanceInput::~UHoudiniAssetInstanceInput()
{

}


UHoudiniAssetInstanceInput*
UHoudiniAssetInstanceInput::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

	// Get name of this input. For the time being we only support geometry inputs.
	HAPI_ObjectInfo ObjectInfo;
	if(HAPI_RESULT_SUCCESS != HAPI_GetObjects(InHoudiniAssetComponent->GetAssetId(), &ObjectInfo, InObjectId, 1))
	{
		return HoudiniAssetInstanceInput;
	}

	// If this is an attribute instancer, see if attribute exists.
	bool bAttributeCheck = UHoudiniAssetInstanceInput::CheckInstanceAttribute(InHoudiniAssetComponent->GetAssetId(), InObjectId, InGeoId, InPartId);

	// This is invalid combination, no object to instance and input is not an attribute instancer.
	if(!bAttributeCheck && -1 == ObjectInfo.objectToInstanceId)
	{
		return HoudiniAssetInstanceInput;
	}

	HoudiniAssetInstanceInput = NewObject<UHoudiniAssetInstanceInput>(InHoudiniAssetComponent);

	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->SetNameAndLabel(ObjectInfo.nameSH);
	HoudiniAssetInstanceInput->SetObjectGeoPartIds(InObjectId, InGeoId, InPartId);
	HoudiniAssetInstanceInput->ObjectToInstanceId = ObjectInfo.objectToInstanceId;

	// Check if this instancer is an attribute instancer and if it is, mark it as such.
	HoudiniAssetInstanceInput->bAttributeInstancer = bAttributeCheck;

	return HoudiniAssetInstanceInput;
}


bool
UHoudiniAssetInstanceInput::CreateInstanceInput()
{
	// Retrieve instance transforms (for each point).
	TArray<FTransform> AllTransforms;
	FHoudiniEngineUtils::HapiGetInstanceTransforms(HoudiniAssetComponent->GetAssetId(), ObjectId, GeoId, PartId, AllTransforms);

	// Store old tuple size.
	int32 OldTupleSize = TupleSize;

	if(bAttributeInstancer)
	{
		HAPI_AttributeInfo ResultAttributeInfo;
		TArray<FString> PointInstanceValues;

		if(!FHoudiniEngineUtils::HapiGetAttributeDataAsString(HoudiniAssetComponent->GetAssetId(), ObjectId, GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE, ResultAttributeInfo, PointInstanceValues))
		{
			// This should not happen - attribute exists, but there was an error retrieving it.
			check(false);
			return false;
		}

		// Instance attribute exists on points.

		// Number of points must match number of transforms.
		if(PointInstanceValues.Num() != AllTransforms.Num())
		{
			// This should not happen!
			check(false);
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
		for(TMultiMap<FString, FHoudiniGeoPartObject>::TIterator IterInstancer(ObjectsToInstance); IterInstancer; ++IterInstancer)
		{
			const FString& ObjectInstancePath = IterInstancer.Key();
			const FHoudiniGeoPartObject& HoudiniGeoPartObject = IterInstancer.Value();

			// Set component transformation.
			InstancedStaticMeshComponents[GeoIdx]->SetRelativeTransform(FTransform(HoudiniGeoPartObject.TransformMatrix));

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
			SetComponentInstanceTransformations(InstancedStaticMeshComponents[GeoIdx], ObjectTransforms);

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
		check(TupleSize);
		AdjustMeshComponentResources(TupleSize, OldTupleSize);

		// Process each existing detected object that needs to be instanced.
		for(int32 GeoIdx = 0; GeoIdx < ObjectsToInstance.Num(); ++GeoIdx)
		{
			const FHoudiniGeoPartObject& HoudiniGeoPartObject = ObjectsToInstance[GeoIdx];

			// Set component transformation.
			InstancedStaticMeshComponents[GeoIdx]->SetRelativeTransform(FTransform(HoudiniGeoPartObject.TransformMatrix));

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
			SetComponentInstanceTransformations(InstancedStaticMeshComponents[GeoIdx], AllTransforms);
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

		UInstancedStaticMeshComponent* Component = ConstructObject<UInstancedStaticMeshComponent>(UInstancedStaticMeshComponent::StaticClass(),
																								  HoudiniAssetComponent->GetOwner(), NAME_None, RF_Transient);

		Component->SetRelativeTransform(FTransform(HoudiniGeoPartObject.TransformMatrix));

		Component->AttachTo(HoudiniAssetComponent);

		SetComponentInstanceTransformations(Component, InstancedTransforms[Idx]);
		InstancedStaticMeshComponents[Idx] = Component;

		// Locate default / original mesh.
		OriginalStaticMeshes[Idx] = HoudiniAssetComponent->LocateStaticMesh(HoudiniGeoPartObject);

		// If custom mesh is not used, use original.
		if(!StaticMeshes[Idx])
		{
			StaticMeshes[Idx] = OriginalStaticMeshes[Idx];
		}

		// Set mesh for this component.
		Component->SetStaticMesh(StaticMeshes[Idx]);
		Component->SetVisibility(true);
		Component->RegisterComponent();
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
UHoudiniAssetInstanceInput::CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo)
{
	// This implementation is not a true parameter. This method should not be called.
	check(false);
	return false;
}


void
UHoudiniAssetInstanceInput::CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
	Super::CreateWidget(DetailCategoryBuilder);

	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(GetParameterLabel())
							.ToolTipText(GetParameterLabel())
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	InputWidgets.SetNum(TupleSize);

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		VerticalBox->AddSlot().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(SAssetDropTarget)
				.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this, &UHoudiniAssetInstanceInput::OnAssetDropped, Idx))
				.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this, &UHoudiniAssetInstanceInput::OnIsAssetAcceptableForDrop, Idx))
				.ToolTipText(GetParameterLabel())
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().Padding(2, 2)
					[
						SAssignNew(InputWidgets[Idx], SAssetSearchBox)
						.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UHoudiniAssetInstanceInput::SetValueCommitted, Idx))
						.OnTextChanged(FOnTextChanged::CreateUObject(this, &UHoudiniAssetInstanceInput::OnValueChanged, Idx))
					]
				]
			]
		];

		UStaticMesh* StaticMesh = StaticMeshes[Idx];
		if(StaticMesh)
		{
			FString AssetName = StaticMesh->GetName();
			InputWidgets[Idx]->SetText(FText::FromString(AssetName));
		}
	}

	Row.ValueWidget.Widget = VerticalBox;
}


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
	Ar << bAttributeInstancer;

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

		GeoPartObjects.SetNumZeroed(TupleSize);
	}

	for(int32 Idx = 0; Idx < TupleSize; ++Idx)
	{
		bool bValidComponent = false;

		if(Ar.IsSaving())
		{
			bValidComponent = (InstancedStaticMeshComponents[Idx] != nullptr);
		}

		Ar << bValidComponent;

		if(bValidComponent)
		{
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
					StaticMeshes[Idx] = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshPathName, nullptr, LOAD_NoWarn, nullptr));
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


void
UHoudiniAssetInstanceInput::OnAssetDropped(UObject* Object, int32 Idx)
{
	if(Object)
	{
		UStaticMesh* InputStaticMesh = Cast<UStaticMesh>(Object);
		if(InputStaticMesh)
		{
			// Change used static mesh.
			StaticMeshes[Idx] = InputStaticMesh;

			FString AssetName = StaticMeshes[Idx]->GetName();
			InputWidgets[Idx]->SetText(FText::FromString(AssetName));

			// Change component's mesh.
			ChangeInstancedStaticMeshComponentMesh(Idx);
		}
	}
}


bool
UHoudiniAssetInstanceInput::OnIsAssetAcceptableForDrop(const UObject* Object, int32 Idx)
{
	// We will accept only static meshes as inputs to instancers.
	if(Object && Object->IsA(UStaticMesh::StaticClass()))
	{
		return true;
	}

	return false;
}


void
UHoudiniAssetInstanceInput::SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 Idx)
{

}


void
UHoudiniAssetInstanceInput::OnValueChanged(const FText& InValue, int32 Idx)
{
	FString AssetName = TEXT("");
	bool bChanged = false;

	if(InValue.IsEmpty())
	{
		// Widget has been cleared, we reset to original mesh.
		StaticMeshes[Idx] = OriginalStaticMeshes[Idx];
		AssetName = StaticMeshes[Idx]->GetName();
		bChanged = true;
	}
	else
	{
		// Otherwise set back the old text.
		UStaticMesh* StaticMesh = StaticMeshes[Idx];
		if(StaticMesh)
		{
			AssetName = StaticMesh->GetName();
		}
	}

	InputWidgets[Idx]->SetText(FText::FromString(AssetName));

	if(bChanged)
	{
		// Change component's mesh.
		ChangeInstancedStaticMeshComponentMesh(Idx);
	}
}


bool
UHoudiniAssetInstanceInput::IsAttributeInstancer() const
{
	return bAttributeInstancer;
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
		InputWidgets.SetNum(ObjectCount);
	}
	else if(ObjectCount > OldTupleSize)
	{
		int32 OldComponentCount = InstancedStaticMeshComponents.Num();

		// If we have less than supplied object count, we need to create new resources.
		InstancedStaticMeshComponents.SetNumZeroed(ObjectCount);
		StaticMeshes.SetNumZeroed(ObjectCount);
		OriginalStaticMeshes.SetNumZeroed(ObjectCount);
		InputWidgets.SetNumZeroed(ObjectCount);

		for(int32 Idx = OldComponentCount; Idx < ObjectCount; ++Idx)
		{
			// We need to create instanced component.
			UInstancedStaticMeshComponent* Component = ConstructObject<UInstancedStaticMeshComponent>(UInstancedStaticMeshComponent::StaticClass(),
																									  HoudiniAssetComponent->GetOwner(), NAME_None, RF_Transient);
			Component->AttachTo(HoudiniAssetComponent);
			Component->RegisterComponent();
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
UHoudiniAssetInstanceInput::SetComponentInstanceTransformations(UInstancedStaticMeshComponent* InstancedStaticMeshComponent, const TArray<FTransform>& InstanceTransforms)
{
	InstancedStaticMeshComponent->ClearInstances();

	for(int32 InstanceIdx = 0; InstanceIdx < InstanceTransforms.Num(); ++InstanceIdx)
	{
		const FTransform& Transform = InstanceTransforms[InstanceIdx];
		const FVector& Scale3D = Transform.GetScale3D();

		// Make sure inverse matrix exists - seems to be a bug in Unreal when submitting instances. Happens in blueprint as well.
		if(!Scale3D.IsNearlyZero(SMALL_NUMBER))
		{
			InstancedStaticMeshComponent->AddInstance(Transform);
		}
	}
}


void
UHoudiniAssetInstanceInput::GetPathInstaceTransforms(const FString& ObjectInstancePath, const TArray<FString>& PointInstanceValues,
													 const TArray<FTransform>& Transforms, TArray<FTransform>& OutTransforms)
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


bool
UHoudiniAssetInstanceInput::CheckInstanceAttribute(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId)
{
	if(-1 == InAssetId || -1 == InObjectId || -1 == InGeoId || -1 == InPartId)
	{
		return false;
	}

	return FHoudiniEngineUtils::HapiCheckAttributeExists(InAssetId, InObjectId, InGeoId, InPartId, HAPI_UNREAL_ATTRIB_INSTANCE, HAPI_ATTROWNER_POINT);
}

