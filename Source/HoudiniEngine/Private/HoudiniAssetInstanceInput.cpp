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

	HoudiniAssetInstanceInput = new UHoudiniAssetInstanceInput(FPostConstructInitializeProperties());

	HoudiniAssetInstanceInput->SetNameAndLabel(ObjectInfo.nameSH);
	HoudiniAssetInstanceInput->HoudiniAssetComponent = InHoudiniAssetComponent;
	HoudiniAssetInstanceInput->SetObjectGeoPartIds(InObjectId, InGeoId, InPartId);
	HoudiniAssetInstanceInput->ObjectToInstanceId = ObjectInfo.objectToInstanceId;

	// Check if this instancer is an attribute instancer and if it is, mark it as such.
	HoudiniAssetInstanceInput->CheckInstanceAttribute();

	return HoudiniAssetInstanceInput;
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
	/*
	FDetailWidgetRow& Row = DetailCategoryBuilder.AddCustomRow(TEXT(""));

	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(Label)
							.ToolTipText(Label)
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	HorizontalBox->AddSlot()
	[
		SNew(SAssetDropTarget)
			.OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(this, &UHoudiniAssetInstanceInput::OnAssetDropped))
			.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateUObject(this, &UHoudiniAssetInstanceInput::OnIsAssetAcceptableForDrop))
			.ToolTipText(Label)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().Padding(2, 2)
			[
				SAssignNew(InputWidget, SAssetSearchBox)
				.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UHoudiniAssetInstanceInput::SetValueCommitted))
			]
		]
	];

	if(StaticMesh)
	{
		FString AssetName = StaticMesh->GetName();
		InputWidget->SetText(FText::FromString(AssetName));
	}

	Row.ValueWidget.Widget = HorizontalBox;
	*/
}


bool
UHoudiniAssetInstanceInput::UploadParameterValue()
{
	/*
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
	}
	else if(HasConnectedAsset())
	{
		// We do not have an object bound.
		DestroyHoudiniAsset();
	}
	*/

	return Super::UploadParameterValue();
}


void
UHoudiniAssetInstanceInput::Serialize(FArchive& Ar)
{
	// Call base implementation.
	Super::Serialize(Ar);
}


void
UHoudiniAssetInstanceInput::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Cast<UHoudiniAssetInstanceInput>(InThis);
	if(HoudiniAssetInstanceInput && !HoudiniAssetInstanceInput->IsPendingKill())
	{
		// Add references for used static meshes.
		for(TArray<UStaticMesh*>::TIterator IterMeshes(HoudiniAssetInstanceInput->StaticMeshes); IterMeshes; ++IterMeshes)
		{
			UStaticMesh* StaticMesh = *IterMeshes;
			Collector.AddReferencedObject(StaticMesh, InThis);
		}

		// Add references for used instanced mesh components.
		for(TArray<UInstancedStaticMeshComponent*>::TIterator IterComponents(HoudiniAssetInstanceInput->InstancedStaticMeshComponents); IterComponents; ++IterComponents)
		{
			UInstancedStaticMeshComponent* InstancedStaticMeshComponent = *IterComponents;
			Collector.AddReferencedObject(InstancedStaticMeshComponent, InThis);
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetInstanceInput::OnAssetDropped(UObject* Object, int32 Idx)
{
	/*
	if(Object)
	{
		UStaticMesh* InputStaticMesh = Cast<UStaticMesh>(Object);
		if(InputStaticMesh)
		{
			StaticMesh = InputStaticMesh;
			FString AssetName = StaticMesh->GetName();
			InputWidget->SetText(FText::FromString(AssetName));

			MarkChanged();
		}
	}
	*/
}


bool
UHoudiniAssetInstanceInput::OnIsAssetAcceptableForDrop(const UObject* Object, int32 Idx)
{
	/*
	// We will accept only static meshes as inputs to instancers.
	if(Object->IsA(UStaticMesh::StaticClass()))
	{
		return true;
	}
	*/

	return false;
}


void
UHoudiniAssetInstanceInput::SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 Idx)
{
	/*
	FString AssetName = TEXT("");
	bool bChanged = false;

	if(ETextCommit::OnCleared)
	{
		// Widget has been cleared.
		StaticMesh = nullptr;
		bChanged = true;
	}
	else
	{
		// Otherwise set back the old text.
		if(StaticMesh)
		{
			AssetName = StaticMesh->GetName();
		}
	}

	InputWidget->SetText(FText::FromString(AssetName));

	if(bChanged)
	{
		MarkChanged();
	}
	*/
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
UHoudiniAssetInstanceInput::CheckInstanceAttribute()
{
	if(-1 == ObjectId || -1 == GeoId || -1 == PartId)
	{
		return;
	}

	// See if this is an attribute instancer.
	bAttributeInstancer = FHoudiniEngineUtils::HapiCheckAttributeExists(HoudiniAssetComponent->GetAssetId(), ObjectId, GeoId, PartId,
																		HAPI_UNREAL_ATTRIB_INSTANCE, HAPI_ATTROWNER_POINT);
}

