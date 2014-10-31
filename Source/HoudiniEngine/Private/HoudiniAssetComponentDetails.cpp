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


TSharedRef<IDetailCustomization>
FHoudiniAssetComponentDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniAssetComponentDetails);
}


FHoudiniAssetComponentDetails::FHoudiniAssetComponentDetails()
{

}


void
FHoudiniAssetComponentDetails::CreateStaticMeshes()
{
	if(HoudiniAssetComponents.Num() > 0)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetComponents[0];

		int32 MeshIdx = 0;
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(HoudiniAssetComponent->StaticMeshes); Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UStaticMesh* StaticMesh = Iter.Value();

			UStaticMesh* OutStaticMesh = FHoudiniEngineUtils::BakeStaticMesh(HoudiniAssetComponent->HoudiniAsset, StaticMesh, MeshIdx);
			MeshIdx++;

			// Notify asset registry that we have created assets. This should update the content browser.
			FAssetRegistryModule::AssetCreated(OutStaticMesh);
		}
	}
}


FReply
FHoudiniAssetComponentDetails::OnButtonClickedBake()
{
	CreateStaticMeshes();
	return FReply::Handled();
}


void
FHoudiniAssetComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get all components which are being customized.
	TArray<TWeakObjectPtr<UObject> > ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	// See if we need to enable baking option.
	for(int32 i = 0; i < ObjectsCustomized.Num(); ++i)
	{
		if(ObjectsCustomized[i].IsValid())
		{
			UObject* Object = ObjectsCustomized[i].Get();

			if(Object)
			{
				// We can't use Unreal cast here since we have patched RTTI for component.
				UHoudiniAssetComponent* HoudiniAssetComponent = (UHoudiniAssetComponent*) Object;
				HoudiniAssetComponents.Add(HoudiniAssetComponent);
			}
		}
	}

	// Create buttons for actions.
	DetailBuilder.EditCategory("HoudiniActions", TEXT(""), ECategoryPriority::Important)
		.AddCustomRow(TEXT(""))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0, 2.0f, 0, 0)
			.FillHeight(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.OnClicked(this, &FHoudiniAssetComponentDetails::OnButtonClickedBake)
						.Text(LOCTEXT("BakeHoudiniActor", "Bake"))
						.ToolTipText( LOCTEXT("BakeHoudiniActorToolTip", "Bake selected Houdini Actor into static meshes"))
					]
			]
		];

	// Create Houdini Asset category.
	DetailBuilder.EditCategory("HoudiniAsset", TEXT(""), ECategoryPriority::Important);

	// Create Houdini Inputs.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniDragAndDropInputs", TEXT(""), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(HoudiniAssetComponent->Inputs); IterInputs; ++IterInputs)
			{
				UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
				HoudiniAssetInput->CreateWidget(DetailCategoryBuilder);
			}
		}
	}

	// Create Houdini Instanced Inputs category.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniDragAndDropInstancedInputs", TEXT(""), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator IterInstancedInputs(HoudiniAssetComponent->InstanceInputs); IterInstancedInputs; ++IterInstancedInputs)
			{
				UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstancedInputs.Value();
				HoudiniAssetInstanceInput->CreateWidget(DetailCategoryBuilder);
			}
		}
	}

	// Create Houdini properties.
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniParameters", TEXT(""), ECategoryPriority::Important);
		for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
		{
			UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
			for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
			{
				UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
				HoudiniAssetParameter->CreateWidget(DetailCategoryBuilder);
			}
		}
	}
}
