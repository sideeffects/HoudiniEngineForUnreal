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
	Test = 0.5f;
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

	// Create properties.
	IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("HoudiniTest", TEXT(""), ECategoryPriority::Important);
	for(TArray<UHoudiniAssetComponent*>::TIterator IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
	{
		UHoudiniAssetComponent* HoudiniAssetComponent = *IterComponents;
		for(TMap<uint32, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			HoudiniAssetParameter->CreateWidget(DetailCategoryBuilder);
		}
	}


	/*
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("HoudiniTest", TEXT(""), ECategoryPriority::Important);
	FDetailWidgetRow& Row = Category.AddCustomRow(TEXT(""));
	Row.NameWidget.Widget = SNew(STextBlock)
							.Text(LOCTEXT("One", "OneOne"))
							.ToolTipText(LOCTEXT("OneTool", "OneOneTool"));
	Row.ValueWidget.Widget = SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.0f)
					.MaxValue(1.0f)
					.MinSliderValue(0.0f)
					.MaxSliderValue(1.0f)
					.OnValueChanged(this, &FHoudiniAssetComponentDetails::SetTestValue, 33)
					//.Value(this, &FHoudiniAssetComponentDetails::TestValue)
					.Value(Test)
					.OnEndSliderMovement(this, &FHoudiniAssetComponentDetails::SetTestValue, 33)
					.OnValueCommitted(this, &FHoudiniAssetComponentDetails::SetTestValueCommitted)
					.Delta(0.1f)
					.SliderExponent(1.0f);
	*/

		/*
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			//.ResizeMode(ESplitterResizeMode::Fixed)
			+SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("One", "OneOne"))
				.ToolTipText(LOCTEXT("OneTool", "OneOneTool"))
			]
			+SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.0f)
					.MaxValue(1.0f)
					.MinSliderValue(0.0f)
					.MaxSliderValue(1.0f)
					.OnValueChanged(this, &FHoudiniAssetComponentDetails::SetTestValue, 33)
					.Value(this, &FHoudiniAssetComponentDetails::TestValue)
					.OnEndSliderMovement(this, &FHoudiniAssetComponentDetails::SetTestValue, 33)
					.OnValueCommitted(this, &FHoudiniAssetComponentDetails::SetTestValueCommitted)
					.Delta(0.1f)
					.SliderExponent(1.0f)
			]
		];

	Category.AddCustomRow(TEXT(""))
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			//.ResizeMode(ESplitterResizeMode::Fixed)
			+SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Two", "TwoTwo"))
				.ToolTipText(LOCTEXT("TwoTool", "TwoTwoTool"))
			]
			+SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.0f)
					.MaxValue(1.0f)
					.MinSliderValue(0.0f)
					.MaxSliderValue(1.0f)
					.OnValueChanged(this, &FHoudiniAssetComponentDetails::SetTestValue, 42)
					.Value(this, &FHoudiniAssetComponentDetails::TestValue)
					.OnEndSliderMovement(this, &FHoudiniAssetComponentDetails::SetTestValue, 42)
					.OnValueCommitted(this, &FHoudiniAssetComponentDetails::SetTestValueCommitted)
					.Delta(0.1f)
					.SliderExponent(1.0f)
			]
		];
		*/

		/*
		.AddCustomRow(TEXT(""))
		[
			SNew(SWidgetSwitcher)
			+SWidgetSwitcher::Slot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("One", "OneOne"))
					.ToolTipText(LOCTEXT("OneTool", "OneOneTool"))
				]
			+SWidgetSwitcher::Slot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Two", "TwoTwo"))
					.ToolTipText(LOCTEXT("TwoTool", "TwoTwoTool"))
				]
		];
		*/

	/*
	DetailBuilder.EditCategory("HoudiniTest", TEXT(""), ECategoryPriority::Important)
		.AddCustomRow(TEXT(""))
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			.ResizeMode(ESplitterResizeMode::Fixed)
			+SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(STextBlock)
					.Text(LOCTEXT("RepositoryLabel", "Repository"))
					.ToolTipText(LOCTEXT("RepositoryLabel_Tooltip", "Address of SVN repository"))
			]
			+SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					//.Label(TEXT("Label"))
					//..LabelVAlign(VAlign_Center)
					.MinValue(0.0f)
					.MaxValue(1.0f)
					.MinSliderValue(0.0f)
					.MaxSliderValue(1.0f)
					.OnValueChanged(this, &FHoudiniAssetComponentDetails::SetTestValue, 42)
					//.Value(0.5f)
					.Value(this, &FHoudiniAssetComponentDetails::TestValue)
					.OnEndSliderMovement(this, &FHoudiniAssetComponentDetails::SetTestValue, 42)
					.OnValueCommitted(this, &FHoudiniAssetComponentDetails::SetTestValueCommitted)
					.Delta(0.1f)
					.SliderExponent(1.0f)
			]
		];
	*/

	//TDelegateInstanceInterface* DelegateInstance 

	// Create Houdini Asset category.
	DetailBuilder.EditCategory("HoudiniAsset", TEXT(""), ECategoryPriority::Important);

	// Create Houdini Inputs category.
	DetailBuilder.EditCategory("HoudiniInputs", TEXT(""), ECategoryPriority::Important);

	// Create Houdini Instance Inputs category.
	DetailBuilder.EditCategory("HoudiniInstancedInputs", TEXT(""), ECategoryPriority::Important);

	// Create Houdini Properties category.
	DetailBuilder.EditCategory("HoudiniProperties", TEXT(""), ECategoryPriority::Important);
}


//float FHoudiniAssetComponentDetails::TestValue() const
TOptional<float> FHoudiniAssetComponentDetails::TestValue() const
{
	return TOptional<float>(Test);
}


void
FHoudiniAssetComponentDetails::SetTestValue(float f, int zid)
{
	Test = f;
}


void
FHoudiniAssetComponentDetails::SetTestValueCommitted(float f, ETextCommit::Type t)
{
	/*
	Default,
	OnEnter,
	OnUserMovedFocus,
	OnCleared
	*/
	//Test = f;
}
