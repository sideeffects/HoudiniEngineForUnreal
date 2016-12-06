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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniApi.h"
#include "HoudiniAssetLogWidget.h"
#include "HoudiniEngineString.h"

uint32
GetTypeHash( TPair< UStaticMesh *, int32 > Pair )
{
    return PointerHash( Pair.Key, Pair.Value );
}

TSharedRef< IDetailCustomization >
FHoudiniAssetComponentDetails::MakeInstance()
{
    return MakeShareable( new FHoudiniAssetComponentDetails );
}

FHoudiniAssetComponentDetails::FHoudiniAssetComponentDetails()
{

}


FHoudiniAssetComponentDetails::~FHoudiniAssetComponentDetails()
{

}

void
FHoudiniAssetComponentDetails::CustomizeDetails( IDetailLayoutBuilder & DetailBuilder )
{
    // Get all components which are being customized.
    TArray< TWeakObjectPtr< UObject > > ObjectsCustomized;
    DetailBuilder.GetObjectsBeingCustomized( ObjectsCustomized );

    // See if we need to enable baking option.
    for ( int32 i = 0; i < ObjectsCustomized.Num(); ++i )
    {
        if ( ObjectsCustomized[ i ].IsValid() )
        {
            UObject * Object = ObjectsCustomized[ i ].Get();

            if( Object )
            {
                UHoudiniAssetComponent * HoudiniAssetComponent = Cast< UHoudiniAssetComponent >( Object );
                HoudiniAssetComponents.Add( HoudiniAssetComponent );
            }
        }
    }

    // Create Houdini parameters.
    {
        IDetailCategoryBuilder & DetailCategoryBuilder =
            DetailBuilder.EditCategory( "HoudiniParameters", FText::GetEmpty(), ECategoryPriority::Important );

        // If we are running Houdini Engine Indie license, we need to display special label.
        if ( FHoudiniEngineUtils::IsLicenseHoudiniEngineIndie() )
        {
            FText ParameterLabelText =
                FText::FromString( TEXT( "Houdini Engine Indie - For Limited Commercial Use Only" ) );

            FSlateFontInfo LargeDetailsFont = IDetailLayoutBuilder::GetDetailFontBold();
            LargeDetailsFont.Size += 2;

            FSlateColor LabelColor = FLinearColor( 1.0f, 1.0f, 0.0f, 1.0f );

            DetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
            [
                SNew( STextBlock )
                .Text( ParameterLabelText )
                .ToolTipText( ParameterLabelText )
                .Font( LargeDetailsFont )
                .Justification( ETextJustify::Center )
                .ColorAndOpacity( LabelColor )
            ];

            DetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
            [
                SNew( SVerticalBox )
                +SVerticalBox::Slot()
                .Padding( 0, 0, 5, 0 )
                [
                    SNew( SSeparator )
                    .Thickness( 2.0f )
                ]
            ];
        }

        for ( TArray< UHoudiniAssetComponent * >::TIterator
            IterComponents( HoudiniAssetComponents ); IterComponents; ++IterComponents )
        {
            UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
            for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator
                IterParams( HoudiniAssetComponent->Parameters ); IterParams; ++IterParams )
            {
                UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();

                // We want to create root parameters here; they will recursively create child parameters.
                if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsChildParameter() &&
                    !HoudiniAssetParameter->IsPendingKill() )
                {
                    HoudiniAssetParameter->CreateWidget( DetailCategoryBuilder );
                }
            }
        }
    }

    // Create Houdini Inputs.
    {
        IDetailCategoryBuilder & DetailCategoryBuilder = DetailBuilder.EditCategory(
            "HoudiniInputs", FText::GetEmpty(), ECategoryPriority::Important );
        for ( TArray< UHoudiniAssetComponent * >::TIterator
            IterComponents( HoudiniAssetComponents ); IterComponents; ++IterComponents )
        {
            UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
            for ( TArray< UHoudiniAssetInput * >::TIterator
                IterInputs( HoudiniAssetComponent->Inputs ); IterInputs; ++IterInputs )
            {
                UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
                if( HoudiniAssetInput )
                {
                    HoudiniAssetInput->CreateWidget( DetailCategoryBuilder );
                }
            }
        }
    }

    // Create Houdini Instanced Inputs category.
    {
        IDetailCategoryBuilder & DetailCategoryBuilder = DetailBuilder.EditCategory(
            "HoudiniInstancedInputs", FText::GetEmpty(), ECategoryPriority::Important );
        for ( TArray< UHoudiniAssetComponent * >::TIterator
            IterComponents( HoudiniAssetComponents ); IterComponents; ++IterComponents )
        {
            UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
            for ( auto& InstanceInput : HoudiniAssetComponent->InstanceInputs )
            {
                if ( InstanceInput )
                {
                    InstanceInput->CreateWidget( DetailCategoryBuilder );
                }
            }
        }
    }

    // Create Houdini Asset category.
    {
        IDetailCategoryBuilder & DetailCategoryBuilder =
            DetailBuilder.EditCategory( "HoudiniAsset", FText::GetEmpty(), ECategoryPriority::Important );
        CreateHoudiniAssetWidget( DetailCategoryBuilder );
    }

    // Create category for generated static meshes and their materials.
    {
        IDetailCategoryBuilder & DetailCategoryBuilder =
            DetailBuilder.EditCategory( "HoudiniGeneratedMeshes", FText::GetEmpty(), ECategoryPriority::Important );
        CreateStaticMeshAndMaterialWidgets( DetailCategoryBuilder );
    }

    // Create Houdini Generated Static mesh settings category.
    DetailBuilder.EditCategory( "HoudiniGeneratedStaticMeshSettings", FText::GetEmpty(), ECategoryPriority::Important );
}

void
FHoudiniAssetComponentDetails::CreateStaticMeshAndMaterialWidgets( IDetailCategoryBuilder & DetailCategoryBuilder )
{
    StaticMeshThumbnailBorders.Empty();
    MaterialInterfaceComboButtons.Empty();
    MaterialInterfaceThumbnailBorders.Empty();

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

    for ( TArray< UHoudiniAssetComponent * >::TIterator
        IterComponents( HoudiniAssetComponents ); IterComponents; ++IterComponents)
    {
        int32 MeshIdx = 0;
        UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;

        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator
            IterMeshes( HoudiniAssetComponent->StaticMeshes ); IterMeshes; ++IterMeshes )
        {
            UStaticMesh * StaticMesh = IterMeshes.Value();
            FHoudiniGeoPartObject & HoudiniGeoPartObject = IterMeshes.Key();

            if ( !StaticMesh )
                continue;

            FString Label = TEXT( "" );
            if ( HoudiniGeoPartObject.HasCustomName() )
                Label = HoudiniGeoPartObject.PartName;
            else
                Label = FString::Printf( TEXT( "Static Mesh %d" ), MeshIdx );

            // Create thumbnail for this mesh.
            TSharedPtr< FAssetThumbnail > StaticMeshThumbnail =
                MakeShareable( new FAssetThumbnail( StaticMesh, 64, 64, AssetThumbnailPool ) );

            TSharedPtr< SBorder > StaticMeshThumbnailBorder;
            TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );
            
            IDetailGroup& StaticMeshGrp = DetailCategoryBuilder.AddGroup(FName(*Label), FText::FromString(Label));
            StaticMeshGrp.AddWidgetRow()
            .NameContent()
            [
                SNew(SSpacer)
                .Size(FVector2D(250, 64))
            ]
            .ValueContent()
            .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                VerticalBox
            ];
            
            VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
            [
                SNew( SHorizontalBox )
                +SHorizontalBox::Slot()
                .Padding( 0.0f, 0.0f, 2.0f, 0.0f )
                .AutoWidth()
                [
                    SAssignNew( StaticMeshThumbnailBorder, SBorder )
                    .Padding( 5.0f )
                    .BorderImage( this, &FHoudiniAssetComponentDetails::GetStaticMeshThumbnailBorder, StaticMesh )
                    .OnMouseDoubleClick( this, &FHoudiniAssetComponentDetails::OnThumbnailDoubleClick, (UObject *) StaticMesh )
                    [
                        SNew( SBox )
                        .WidthOverride( 64 )
                        .HeightOverride( 64 )
                        .ToolTipText( FText::FromString( StaticMesh->GetPathName() ) )
                        [
                            StaticMeshThumbnail->MakeThumbnailWidget()
                        ]
                    ]
                ]
                +SHorizontalBox::Slot()
                .FillWidth( 1.0f )
                .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
                .VAlign( VAlign_Center )
                [
                    SNew( SVerticalBox )
                    +SVerticalBox::Slot()
                    [
                        SNew( SHorizontalBox )
                        +SHorizontalBox::Slot()
                        .MaxWidth( 80.0f )
                        [
                            SNew( SButton )
                            .VAlign( VAlign_Center )
                            .HAlign( HAlign_Center )
                            .Text( LOCTEXT( "Bake", "Bake" ) )
                            .OnClicked( this, &FHoudiniAssetComponentDetails::OnBakeStaticMesh, StaticMesh, HoudiniAssetComponent )
                            .ToolTipText( LOCTEXT( "HoudiniStaticMeshBakeButton", "Bake this generated static mesh" ) )
                        ]
                    ]
                ]
            ];

            // Store thumbnail for this mesh.
            StaticMeshThumbnailBorders.Add( StaticMesh, StaticMeshThumbnailBorder );

            // We need to add material box for each material present in this static mesh.
            auto & StaticMeshMaterials = StaticMesh->StaticMaterials;
            for ( int32 MaterialIdx = 0; MaterialIdx < StaticMeshMaterials.Num(); ++MaterialIdx )
            {
                UMaterialInterface * MaterialInterface = StaticMeshMaterials[ MaterialIdx ].MaterialInterface;
                TSharedPtr< SBorder > MaterialThumbnailBorder;
                TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

                FString MaterialName, MaterialPathName;
                if (MaterialInterface)
                {
                    MaterialName = MaterialInterface->GetName();
                    MaterialPathName = MaterialInterface->GetPathName();
                }

                // Create thumbnail for this material.
                TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
                    MakeShareable( new FAssetThumbnail( MaterialInterface, 64, 64, AssetThumbnailPool ) );

                VerticalBox->AddSlot().Padding( 0, 2 )
                [
                    SNew( SAssetDropTarget )
                    .OnIsAssetAcceptableForDrop( this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceDraggedOver )
                    .OnAssetDropped(
                        this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceDropped,
                        StaticMesh, &HoudiniGeoPartObject, MaterialIdx )
                    [
                        SAssignNew( HorizontalBox, SHorizontalBox )
                    ]
                ];

                HorizontalBox->AddSlot().Padding( 0.0f, 0.0f, 2.0f, 0.0f ).AutoWidth()
                [
                    SAssignNew( MaterialThumbnailBorder, SBorder )
                    .Padding( 5.0f )
                    .BorderImage(
                        this, &FHoudiniAssetComponentDetails::GetMaterialInterfaceThumbnailBorder, StaticMesh, MaterialIdx )
                    .OnMouseDoubleClick(
                        this, &FHoudiniAssetComponentDetails::OnThumbnailDoubleClick, (UObject *) MaterialInterface )
                    [
                        SNew( SBox )
                        .WidthOverride( 64 )
                        .HeightOverride( 64 )
                        .ToolTipText( FText::FromString( MaterialPathName ) )
                        [
                            MaterialInterfaceThumbnail->MakeThumbnailWidget()
                        ]
                    ]
                ];

                // Store thumbnail for this mesh and material index.
                {
                    TPairInitializer< UStaticMesh *, int32 > Pair( StaticMesh, MaterialIdx );
                    MaterialInterfaceThumbnailBorders.Add( Pair, MaterialThumbnailBorder );
                }

                TSharedPtr< SComboButton > AssetComboButton;
                TSharedPtr< SHorizontalBox > ButtonBox;

                HorizontalBox->AddSlot()
                .FillWidth( 1.0f )
                .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
                .VAlign( VAlign_Center )
                [
                    SNew( SVerticalBox )
                    +SVerticalBox::Slot()
                    .HAlign( HAlign_Fill )
                    [
                        SAssignNew( ButtonBox, SHorizontalBox )
                        +SHorizontalBox::Slot()
                        [
                            SAssignNew( AssetComboButton, SComboButton )
                            //.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
                            .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                            .ForegroundColor( FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity" ) )
                            .OnGetMenuContent( this, &FHoudiniAssetComponentDetails::OnGetMaterialInterfaceMenuContent,
                                MaterialInterface, StaticMesh, &HoudiniGeoPartObject, MaterialIdx )
                            .ContentPadding( 2.0f )
                            .ButtonContent()
                            [
                                SNew( STextBlock )
                                .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                                .Font( FEditorStyle::GetFontStyle( FName( TEXT( "PropertyWindow.NormalFont" ) ) ) )
                                .Text( FText::FromString( MaterialName ) )
                            ]
                        ]
                    ]
                ];

                // Create tooltip.
                FFormatNamedArguments Args;
                Args.Add( TEXT( "Asset" ), FText::FromString( MaterialName ) );
                FText MaterialTooltip = FText::Format(
                    LOCTEXT( "BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser" ), Args );

                ButtonBox->AddSlot()
                .AutoWidth()
                .Padding( 2.0f, 0.0f )
                .VAlign( VAlign_Center )
                [
                    PropertyCustomizationHelpers::MakeBrowseButton(
                        FSimpleDelegate::CreateSP(
                            this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceBrowse, MaterialInterface ),
                        TAttribute< FText >( MaterialTooltip ) )
                ];

                ButtonBox->AddSlot()
                .AutoWidth()
                .Padding( 2.0f, 0.0f )
                .VAlign( VAlign_Center )
                [
                    SNew( SButton )
                    .ToolTipText( LOCTEXT( "ResetToBaseMaterial", "Reset to base material" ) )
                    .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                    .ContentPadding( 0 )
                    .Visibility( EVisibility::Visible )
                    .OnClicked(
                        this, &FHoudiniAssetComponentDetails::OnResetMaterialInterfaceClicked,
                        StaticMesh, &HoudiniGeoPartObject, MaterialIdx )
                    [
                        SNew( SImage )
                        .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
                    ]
                ];

                // Store combo button for this mesh and index.
                {
                    TPairInitializer< UStaticMesh *, int32 > Pair( StaticMesh, MaterialIdx );
                    MaterialInterfaceComboButtons.Add( Pair, AssetComboButton );
                }
            }

            MeshIdx++;
        }
    }

    TSharedRef< SHorizontalBox > HorizontalButtonBox = SNew( SHorizontalBox );
    DetailCategoryBuilder.AddCustomRow( FText::GetEmpty() )
    [
        SNew( SVerticalBox )
        +SVerticalBox::Slot()
        .Padding( 0, 2.0f, 0, 0 )
        .FillHeight( 1.0f )
        .VAlign( VAlign_Center )
        [
            SAssignNew( HorizontalButtonBox, SHorizontalBox )
        ]
    ];

    HorizontalButtonBox->AddSlot()
    .AutoWidth()
    .Padding( 2.0f, 0.0f )
    .VAlign( VAlign_Center )
    .HAlign( HAlign_Center )
    [
        SNew( SButton )
        .VAlign( VAlign_Center )
        .HAlign( HAlign_Center )
        .OnClicked( this, &FHoudiniAssetComponentDetails::OnBakeAllStaticMeshes )
        .Text( LOCTEXT( "BakeHoudiniActor", "Bake All" ) )
        .ToolTipText( LOCTEXT( "BakeHoudiniActorToolTip", "Bake all generated static meshes" ) )
    ];
}

void
FHoudiniAssetComponentDetails::CreateHoudiniAssetWidget( IDetailCategoryBuilder & DetailCategoryBuilder )
{
    // Reset Houdini asset related widgets.
    HoudiniAssetComboButton.Reset();
    HoudiniAssetThumbnailBorder.Reset();

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

    FSlateFontInfo NormalFont = FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
    UHoudiniAsset * HoudiniAsset = nullptr;
    UHoudiniAssetComponent * HoudiniAssetComponent = nullptr;
    FString HoudiniAssetPathName = TEXT( "" );
    FString HoudiniAssetName = TEXT( "" );

    if ( HoudiniAssetComponents.Num() > 0 )
    {
        HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
        HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

        if ( HoudiniAsset )
        {
            HoudiniAssetPathName = HoudiniAsset->GetPathName();
            HoudiniAssetName = HoudiniAsset->GetName();
        }
    }

    // Create thumbnail for this Houdini asset.
    TSharedPtr< FAssetThumbnail > HoudiniAssetThumbnail =
        MakeShareable(new FAssetThumbnail(HoudiniAsset, 64, 64, AssetThumbnailPool));

    IDetailGroup& OptionsGroup = DetailCategoryBuilder.AddGroup(TEXT("Options"), LOCTEXT("Options", "Asset Options"));
    OptionsGroup.AddWidgetRow()
    .NameContent()
    [
        SNew(STextBlock)
        .Text(LOCTEXT("HoudiniDigitalAsset", "Houdini Digital Asset"))
        .Font(NormalFont)
    ]
    .ValueContent()
    .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
    [
        SNew( SAssetDropTarget )
        .OnIsAssetAcceptableForDrop( this, &FHoudiniAssetComponentDetails::OnHoudiniAssetDraggedOver )
        .OnAssetDropped( this, &FHoudiniAssetComponentDetails::OnHoudiniAssetDropped )
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 0.0f, 0.0f, 2.0f, 0.0f )
            .AutoWidth()
            [
                SAssignNew( HoudiniAssetThumbnailBorder, SBorder )
                .Padding( 5.0f )
                .BorderImage( this, &FHoudiniAssetComponentDetails::GetHoudiniAssetThumbnailBorder )
                [
                    SNew( SBox )
                    .WidthOverride( 64 )
                    .HeightOverride( 64 )
                    .ToolTipText( FText::FromString( HoudiniAssetPathName ) )
                    [
                        HoudiniAssetThumbnail->MakeThumbnailWidget()
                    ]
                ]
            ]
            +SHorizontalBox::Slot()
            .FillWidth(1.f)
            .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
            .VAlign( VAlign_Center )
            [
                SNew( SVerticalBox )
                +SVerticalBox::Slot()
                .HAlign( HAlign_Fill )
                [
                    SNew( SHorizontalBox )
                    +SHorizontalBox::Slot()
                    [
                        SAssignNew( HoudiniAssetComboButton, SComboButton )
                        .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                        .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
                        .OnGetMenuContent( this, &FHoudiniAssetComponentDetails::OnGetHoudiniAssetMenuContent )
                        .ContentPadding( 2.0f )
                        .ButtonContent()
                        [
                            SNew( STextBlock )
                            .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                            .Font(NormalFont)
                            .Text( FText::FromString( HoudiniAssetName ) )
                        ]
                    ]
                    +SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding( 2.0f, 0.0f )
                    .VAlign( VAlign_Center )
                    [
                        PropertyCustomizationHelpers::MakeBrowseButton(
                            FSimpleDelegate::CreateSP( this, &FHoudiniAssetComponentDetails::OnHoudiniAssetBrowse ),
                            TAttribute< FText >( FText::FromString( HoudiniAssetName ) ) )
                    ]
                    +SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding( 2.0f, 0.0f )
                    .VAlign( VAlign_Center )
                    [
                        SNew( SButton )
                        .ToolTipText( LOCTEXT( "ResetToBaseHoudiniAsset", "Reset Houdini Asset" ) )
                        .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                        .ContentPadding( 0 )
                        .Visibility( EVisibility::Visible )
                        .OnClicked( this, &FHoudiniAssetComponentDetails::OnResetHoudiniAssetClicked )
                        [
                            SNew( SImage )
                            .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
                        ]
                    ]  // horizontal buttons next to thumbnail box
                ]
            ]  // horizontal asset chooser box
        ]
    ];

    auto AddOptionRow = [&](const FText& NameText, FOnCheckStateChanged OnCheckStateChanged, TAttribute<ECheckBoxState> IsCheckedAttr)
    {
        OptionsGroup.AddWidgetRow()
        .NameContent()
        [
            SNew(STextBlock)
            .Text(NameText)
            .Font(NormalFont)
        ]
        .ValueContent()
        [
            SNew( SCheckBox )
            .OnCheckStateChanged( OnCheckStateChanged )
            .IsChecked( IsCheckedAttr )
        ];
    };

    AddOptionRow(
        LOCTEXT("HoudiniEnableCookingOnParamChange", "Enable Cooking on Parameter Change"), 
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingCooking, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingCooking, HoudiniAssetComponent)));
    AddOptionRow(
        LOCTEXT("HoudiniUploadTransformsToHoudiniEngine", "Upload Transforms to Houdini Engine"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingUploadTransform, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingUploadTransform, HoudiniAssetComponent)));
    AddOptionRow(
        LOCTEXT("HoudiniTransformChangeTriggersCooks", "Transform Change Triggers Cooks"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingTransformCooking, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingTransformCooking, HoudiniAssetComponent)));
    AddOptionRow(
        LOCTEXT("HoudiniUseHoudiniMaterials", "Use Native Houdini Materials"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingUseHoudiniMaterials, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingUseHoudiniMaterials, HoudiniAssetComponent)));
    AddOptionRow(
        LOCTEXT("HoudiniCookingTriggersDownstreamCooks", "Cooking Triggers Downstream Cooks"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingCookingTriggersDownstreamCooks, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetComponentDetails::IsCheckedComponentSettingCookingTriggersDownstreamCooks, HoudiniAssetComponent)));

    auto ActionButtonSlot = [&](const FText& InText, const FText& InToolTipText, FOnClicked InOnClicked) -> SHorizontalBox::FSlot&
    {
        return SHorizontalBox::Slot()
        .AutoWidth()
        .Padding( 2.0f, 0.0f )
        .VAlign( VAlign_Center )
        .HAlign( HAlign_Center )
        [
            SNew( SButton )
            .VAlign( VAlign_Center )
            .HAlign( HAlign_Center )
            .OnClicked(InOnClicked)
            .Text(InText)
            .ToolTipText(InToolTipText)
        ];
    };

    IDetailGroup& CookGroup = DetailCategoryBuilder.AddGroup(TEXT("Cooking"), LOCTEXT("CookingActions", "Cooking Actions"));
    CookGroup.AddWidgetRow()
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        +ActionButtonSlot(
            LOCTEXT("RecookHoudiniActor", "Recook Asset"), 
            LOCTEXT("RecookHoudiniActorToolTip", "Recooks the outputs of the Houdini asset"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnRecookAsset))
        +ActionButtonSlot(
            LOCTEXT("RebuildHoudiniActor", "Rebuild Asset"),
            LOCTEXT("RebuildHoudiniActorToolTip", "Deletes and then re-creates and re-cooks the Houdini asset"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnRebuildAsset))
        +ActionButtonSlot(
            LOCTEXT("ResetHoudiniActor", "Reset Parameters"),
            LOCTEXT("ResetHoudiniActorToolTip", "Resets parameters to their default values"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnResetAsset))
    ];

    IDetailGroup& BakeGroup = DetailCategoryBuilder.AddGroup(TEXT("Baking"), LOCTEXT("BakingActions", "Baking Actions"));
    TSharedPtr< SButton > BakeToInputButton;
    BakeGroup.AddWidgetRow()
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        +ActionButtonSlot(
            LOCTEXT("BakeBlueprintHoudiniActor", "Bake Blueprint"),
            LOCTEXT("BakeBlueprintHoudiniActorToolTip", "Bakes to a new Blueprint"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnBakeBlueprint))
        +ActionButtonSlot(
            LOCTEXT("BakeReplaceBlueprintHoudiniActor", "Replace With Blueprint"),
            LOCTEXT("BakeReplaceBlueprintHoudiniActorToolTip", "Bakes to a new Blueprint and replaces this Actor"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnBakeBlueprintReplace))
        +ActionButtonSlot(
            LOCTEXT("BakeToActors", "Bake to Actors"),
            LOCTEXT("BakeToActorsTooltip", "Bakes each output and creates new individual Actors"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnBakeToActors))
        +SHorizontalBox::Slot()
        .AutoWidth()
        .Padding( 2.0f, 0.0f )
        .VAlign( VAlign_Center )
        .HAlign( HAlign_Center )
        [
            SAssignNew(BakeToInputButton, SButton)
            .VAlign( VAlign_Center )
            .HAlign( HAlign_Center )
            .OnClicked(this, &FHoudiniAssetComponentDetails::OnBakeToInput)
            .Text(LOCTEXT("BakeToInput", "Bake to Outliner Input"))
            .ToolTipText(LOCTEXT("BakeToInputTooltip", "Bakes single static mesh and sets it on the first outliner input actor and then disconnects it.\nNote: There must be one static mesh outliner input and one generated mesh."))
        ]
    ];

    BakeToInputButton->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([=] {
        return FHoudiniEngineUtils::GetCanComponentBakeToOutlinerInput(HoudiniAssetComponent);
    })));

    IDetailGroup& HelpGroup = DetailCategoryBuilder.AddGroup(TEXT("Help"), LOCTEXT("Help", "Help and Debugging"));
    HelpGroup.AddWidgetRow()
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        +ActionButtonSlot(
            LOCTEXT("FetchCookLogHoudiniActor", "Fetch Cook Log"),
            LOCTEXT("FetchCookLogHoudiniActorToolTip", "Fetches the cook log from Houdini"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnFetchCookLog))
        +ActionButtonSlot(
            LOCTEXT("FetchAssetHelpHoudiniActor", "Asset Help"),
            LOCTEXT("FetchAssetHelpHoudiniActorToolTip", "Displays the asset's Help text"),
            FOnClicked::CreateSP(this, &FHoudiniAssetComponentDetails::OnFetchAssetHelp, HoudiniAssetComponent))
    ];
}

const FSlateBrush *
FHoudiniAssetComponentDetails::GetStaticMeshThumbnailBorder( UStaticMesh * StaticMesh ) const
{
    TSharedPtr< SBorder > ThumbnailBorder = StaticMeshThumbnailBorders[ StaticMesh ];
    if ( ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered() )
        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
    else
        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
}

const FSlateBrush *
FHoudiniAssetComponentDetails::GetMaterialInterfaceThumbnailBorder( UStaticMesh * StaticMesh, int32 MaterialIdx ) const
{
    TPairInitializer< UStaticMesh *, int32 > Pair( StaticMesh, MaterialIdx );
    TSharedPtr< SBorder > ThumbnailBorder = MaterialInterfaceThumbnailBorders[ Pair ];

    if ( ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered() )
        return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
    else
        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
}

FReply
FHoudiniAssetComponentDetails::OnThumbnailDoubleClick(
    const FGeometry & InMyGeometry,
    const FPointerEvent & InMouseEvent, UObject * Object )
{
    if ( Object && GEditor )
        GEditor->EditObject( Object );

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnBakeStaticMesh( UStaticMesh * StaticMesh, UHoudiniAssetComponent * HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent && StaticMesh )
    {
        // We need to locate corresponding geo part object in component.
        const FHoudiniGeoPartObject& HoudiniGeoPartObject = HoudiniAssetComponent->LocateGeoPartObject( StaticMesh );

        (void) FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(
            StaticMesh, HoudiniAssetComponent, HoudiniGeoPartObject, true );
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnBakeAllStaticMeshes()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];

        for( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator
            Iter(HoudiniAssetComponent->StaticMeshes); Iter; ++Iter )
        {
            FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
            UStaticMesh * StaticMesh = Iter.Value();
            (void) OnBakeStaticMesh( StaticMesh, HoudiniAssetComponent );
        }
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnRecookAsset()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
        HoudiniAssetComponent->StartTaskAssetCookingManual();
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnRebuildAsset()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
        HoudiniAssetComponent->StartTaskAssetRebuildManual();
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnResetAsset()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
        HoudiniAssetComponent->StartTaskAssetResetManual();
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnBakeBlueprint()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];

        // If component is not cooking or instancing, we can bake blueprint.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
            FHoudiniEngineUtils::BakeBlueprint( HoudiniAssetComponent );
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnBakeBlueprintReplace()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];

        // If component is not cooking or instancing, we can bake blueprint.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
            FHoudiniEngineUtils::ReplaceHoudiniActorWithBlueprint( HoudiniAssetComponent );
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnBakeToActors()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];

        // If component is not cooking or instancing, we can bake.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
        {
            FHoudiniEngineUtils::BakeHoudiniActorToActors( HoudiniAssetComponent, true );
        }
    }

    return FReply::Handled();
}

FReply 
FHoudiniAssetComponentDetails::OnBakeToInput()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];

        // If component is not cooking or instancing, we can bake.
        if ( !HoudiniAssetComponent->IsInstantiatingOrCooking() )
        {
            FHoudiniEngineUtils::BakeHoudiniActorToOutlinerInput( HoudiniAssetComponent );
        }
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnFetchCookLog()
{
    TSharedPtr< SWindow > ParentWindow;

    // Get fetch cook status.
    const FString CookLogString = FHoudiniEngineUtils::GetCookResult();

    // Check if the main frame is loaded. When using the old main frame it may not be.
    if ( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
    {
        IMainFrameModule & MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
        ParentWindow = MainFrame.GetParentWindow();
    }

    if ( ParentWindow.IsValid() )
    {
        TSharedPtr< SHoudiniAssetLogWidget > HoudiniAssetCookLog;

        TSharedRef< SWindow > Window =
            SNew( SWindow )
                .Title( LOCTEXT( "WindowTitle", "Houdini Cook Log" ) )
                .ClientSize( FVector2D( 640, 480 ) );

        Window->SetContent( 
            SAssignNew( HoudiniAssetCookLog, SHoudiniAssetLogWidget )
            .LogText( CookLogString ) );

        FSlateApplication::Get().AddModalWindow( Window, ParentWindow, false );
    }

    return FReply::Handled();
}

FReply
FHoudiniAssetComponentDetails::OnFetchAssetHelp( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent )
    {
        HAPI_AssetInfo AssetInfo;
        HAPI_NodeId AssetId = HoudiniAssetComponent->GetAssetId();

        if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
        {
            auto result = FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo );
            if ( result == HAPI_RESULT_SUCCESS )
            {
                FString HelpLogString = TEXT( "" );
                FHoudiniEngineString HoudiniEngineString( AssetInfo.helpTextSH );
                if ( HoudiniEngineString.ToFString( HelpLogString ) )
                {
                    if ( HelpLogString.IsEmpty() )
                        HelpLogString = TEXT( "No Asset Help Found" );

                    TSharedPtr< SWindow > ParentWindow;

                    // Check if the main frame is loaded. When using the old main frame it may not be.
                    if ( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
                    {
                        IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked< IMainFrameModule >( "MainFrame" );
                        ParentWindow = MainFrame.GetParentWindow();
                    }

                    if ( ParentWindow.IsValid() )
                    {
                        TSharedPtr< SHoudiniAssetLogWidget > HoudiniAssetHelpLog;

                        TSharedRef< SWindow > Window =
                            SNew( SWindow )
                                .Title( LOCTEXT( "WindowTitle", "Houdini Asset Help" ) )
                                .ClientSize( FVector2D( 640, 480 ) );

                        Window->SetContent( 
                            SAssignNew( HoudiniAssetHelpLog, SHoudiniAssetLogWidget )
                            .LogText( HelpLogString ) );

                        FSlateApplication::Get().AddModalWindow( Window, ParentWindow, false );
                    }
                }
            }
        }
    }

    return FReply::Handled();
}

bool
FHoudiniAssetComponentDetails::OnMaterialInterfaceDraggedOver( const UObject * InObject ) const
{
    return ( InObject && InObject->IsA( UMaterialInterface::StaticClass() ) );
}

void
FHoudiniAssetComponentDetails::OnMaterialInterfaceDropped(
    UObject * InObject, UStaticMesh * StaticMesh,
    FHoudiniGeoPartObject * HoudiniGeoPartObject, int32 MaterialIdx )
{
    UMaterialInterface * MaterialInterface = Cast< UMaterialInterface >( InObject );
    if ( MaterialInterface )
    {
        bool bViewportNeedsUpdate = false;

        // Replace material on component using this static mesh.
        for ( TArray< UHoudiniAssetComponent * >::TIterator
            IterComponents( HoudiniAssetComponents ); IterComponents; ++IterComponents )
        {
            UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
            if ( HoudiniAssetComponent )
            {
                bool bMaterialReplaced = false;

                // Retrieve material interface which is being replaced.
                UMaterialInterface * OldMaterialInterface = StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface;

                if ( OldMaterialInterface != MaterialInterface )
                {
                    // Record replaced material.
                    const bool bReplaceSuccessful = HoudiniAssetComponent->ReplaceMaterial(
                        *HoudiniGeoPartObject, MaterialInterface, OldMaterialInterface, MaterialIdx );
                    if ( bReplaceSuccessful )
                    {
                        FScopedTransaction Transaction(
                            TEXT( HOUDINI_MODULE_EDITOR ),
                            LOCTEXT( "HoudiniMaterialReplacement", "Houdini Material Replacement" ), HoudiniAssetComponent );

                        // Replace material on static mesh.
                        StaticMesh->Modify();
                        StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface = MaterialInterface;

                        UStaticMeshComponent * StaticMeshComponent =
                            HoudiniAssetComponent->LocateStaticMeshComponent( StaticMesh );
                        if ( StaticMeshComponent )
                        {
                            StaticMeshComponent->Modify();
                            StaticMeshComponent->SetMaterial( MaterialIdx, MaterialInterface );

                            bMaterialReplaced = true;
                        }
                        
                        TArray< UInstancedStaticMeshComponent * > InstancedStaticMeshComponents;
                        if ( HoudiniAssetComponent->LocateInstancedStaticMeshComponents( StaticMesh, InstancedStaticMeshComponents ) )
                        {
                            for ( int32 Idx = 0; Idx < InstancedStaticMeshComponents.Num(); ++Idx )
                            {
                                UInstancedStaticMeshComponent * InstancedStaticMeshComponent = InstancedStaticMeshComponents[ Idx ];
                                if ( InstancedStaticMeshComponent )
                                {
                                    InstancedStaticMeshComponent->Modify();
                                    InstancedStaticMeshComponent->SetMaterial( MaterialIdx, MaterialInterface );

                                    bMaterialReplaced = true;
                                }
                            }
                        }
                    }
                }

                if ( bMaterialReplaced )
                {
                    HoudiniAssetComponent->UpdateEditorProperties( false );
                    bViewportNeedsUpdate = true;
                }
            }
        }

        if ( GEditor && bViewportNeedsUpdate )
            GEditor->RedrawAllViewports();
    }
}

TSharedRef< SWidget >
FHoudiniAssetComponentDetails::OnGetMaterialInterfaceMenuContent(
    UMaterialInterface * MaterialInterface,
    UStaticMesh * StaticMesh, FHoudiniGeoPartObject * HoudiniGeoPartObject, int32 MaterialIdx )
{
    TArray< const UClass * > AllowedClasses;
    AllowedClasses.Add( UMaterialInterface::StaticClass() );

    TArray< UFactory * > NewAssetFactories;

    return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
        FAssetData( MaterialInterface ), true, AllowedClasses,
        NewAssetFactories, OnShouldFilterMaterialInterface,
        FOnAssetSelected::CreateSP(
            this, &FHoudiniAssetComponentDetails::OnMaterialInterfaceSelected,
            StaticMesh, HoudiniGeoPartObject, MaterialIdx ),
        FSimpleDelegate::CreateSP( this, &FHoudiniAssetComponentDetails::CloseMaterialInterfaceComboButton ) );
}

void
FHoudiniAssetComponentDetails::OnMaterialInterfaceSelected(
    const FAssetData & AssetData, UStaticMesh * StaticMesh,
    FHoudiniGeoPartObject * HoudiniGeoPartObject, int32 MaterialIdx )
{
    TPairInitializer< UStaticMesh *, int32 > Pair( StaticMesh, MaterialIdx );
    TSharedPtr< SComboButton > AssetComboButton = MaterialInterfaceComboButtons[ Pair ];
    if ( AssetComboButton.IsValid() )
    {
        AssetComboButton->SetIsOpen( false );

        UObject * Object = AssetData.GetAsset();
        OnMaterialInterfaceDropped( Object, StaticMesh, HoudiniGeoPartObject, MaterialIdx );
    }
}

void
FHoudiniAssetComponentDetails::CloseMaterialInterfaceComboButton()
{

}

void
FHoudiniAssetComponentDetails::OnMaterialInterfaceBrowse( UMaterialInterface * MaterialInterface )
{
    if ( GEditor )
    {
        TArray< UObject * > Objects;
        Objects.Add( MaterialInterface );
        GEditor->SyncBrowserToObjects( Objects );
    }
}

FReply
FHoudiniAssetComponentDetails::OnResetMaterialInterfaceClicked(
    UStaticMesh * StaticMesh,
    FHoudiniGeoPartObject * HoudiniGeoPartObject,
    int32 MaterialIdx )
{
    bool bViewportNeedsUpdate = false;

    for ( TArray< UHoudiniAssetComponent * >::TIterator
        IterComponents( HoudiniAssetComponents ); IterComponents; ++IterComponents )
    {
        // Retrieve material interface which is being replaced.
        UMaterialInterface * MaterialInterface = StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface;
        UMaterialInterface * MaterialInterfaceReplacement = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

        UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
        if ( HoudiniAssetComponent )
        {
            bool bMaterialRestored = false;

            FString MaterialShopName;
            if ( HoudiniAssetComponent->GetReplacementMaterialShopName(*HoudiniGeoPartObject, MaterialInterface, MaterialShopName ) )
            {
                HoudiniAssetComponent->RemoveReplacementMaterial( *HoudiniGeoPartObject, MaterialShopName );

                UMaterialInterface * AssignedMaterial = HoudiniAssetComponent->GetAssignmentMaterial( MaterialShopName );
                if ( AssignedMaterial )
                    MaterialInterfaceReplacement = AssignedMaterial;
            }

            // Replace material on static mesh.
            StaticMesh->Modify();
            StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface = MaterialInterfaceReplacement;

            UStaticMeshComponent * StaticMeshComponent = HoudiniAssetComponent->LocateStaticMeshComponent( StaticMesh );
            if ( StaticMeshComponent )
            {
                StaticMeshComponent->Modify();
                StaticMeshComponent->SetMaterial( MaterialIdx, MaterialInterfaceReplacement );

                bMaterialRestored = true;
            }

            TArray< UInstancedStaticMeshComponent * > InstancedStaticMeshComponents;
            if ( HoudiniAssetComponent->LocateInstancedStaticMeshComponents( StaticMesh, InstancedStaticMeshComponents ) )
            {
                for ( int32 Idx = 0; Idx < InstancedStaticMeshComponents.Num(); ++Idx )
                {
                    UInstancedStaticMeshComponent * InstancedStaticMeshComponent = InstancedStaticMeshComponents[ Idx ];
                    if ( InstancedStaticMeshComponent )
                    {
                        InstancedStaticMeshComponent->Modify();
                        InstancedStaticMeshComponent->SetMaterial( MaterialIdx, MaterialInterfaceReplacement );

                        bMaterialRestored = true;
                    }
                }
            }

            if ( bMaterialRestored )
            {
                HoudiniAssetComponent->UpdateEditorProperties( false );
                bViewportNeedsUpdate = true;
            }
        }
    }

    if ( GEditor && bViewportNeedsUpdate )
    {
        GEditor->RedrawAllViewports();
    }

    return FReply::Handled();
}

void
FHoudiniAssetComponentDetails::OnHoudiniAssetDropped( UObject * InObject )
{
    if ( InObject )
    {
        UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >( InObject );
        if ( HoudiniAsset && HoudiniAssetComponents.Num() > 0 )
        {
            UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];

            // Assign asset to component.
            HoudiniAssetComponent->SetHoudiniAsset( HoudiniAsset );
        }
    }
}

bool
FHoudiniAssetComponentDetails::OnHoudiniAssetDraggedOver( const UObject * InObject ) const
{
    return ( InObject && InObject->IsA( UHoudiniAsset::StaticClass() ) );
}

const FSlateBrush *
FHoudiniAssetComponentDetails::GetHoudiniAssetThumbnailBorder() const
{
    if ( HoudiniAssetThumbnailBorder.IsValid() && HoudiniAssetThumbnailBorder->IsHovered() )
        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
    else
        return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
}

TSharedRef< SWidget >
FHoudiniAssetComponentDetails::OnGetHoudiniAssetMenuContent()
{
    TArray< const UClass * > AllowedClasses;
    AllowedClasses.Add( UHoudiniAsset::StaticClass() );

    TArray< UFactory * > NewAssetFactories;

    UHoudiniAsset * HoudiniAsset = nullptr;
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
        HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
    }

    return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
        FAssetData( HoudiniAsset ), true,
        AllowedClasses, NewAssetFactories, OnShouldFilterHoudiniAsset,
        FOnAssetSelected::CreateSP( this, &FHoudiniAssetComponentDetails::OnHoudiniAssetSelected ),
        FSimpleDelegate::CreateSP( this, &FHoudiniAssetComponentDetails::CloseHoudiniAssetComboButton ) );
}

void
FHoudiniAssetComponentDetails::OnHoudiniAssetSelected( const FAssetData & AssetData )
{
    if ( HoudiniAssetComboButton.IsValid() )
    {
        HoudiniAssetComboButton->SetIsOpen( false );

        UObject * Object = AssetData.GetAsset();
        OnHoudiniAssetDropped( Object );
    }
}

void
FHoudiniAssetComponentDetails::CloseHoudiniAssetComboButton()
{

}

void
FHoudiniAssetComponentDetails::OnHoudiniAssetBrowse()
{
    if ( GEditor )
    {
        UHoudiniAsset * HoudiniAsset = nullptr;
        if ( HoudiniAssetComponents.Num() > 0 )
        {
            UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
            HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;

            if( HoudiniAsset )
            {
                TArray< UObject * > Objects;
                Objects.Add(HoudiniAsset);
                GEditor->SyncBrowserToObjects( Objects );
            }
        }
    }
}

FReply
FHoudiniAssetComponentDetails::OnResetHoudiniAssetClicked()
{
    if ( HoudiniAssetComponents.Num() > 0 )
    {
        UHoudiniAssetComponent * HoudiniAssetComponent = HoudiniAssetComponents[ 0 ];
        HoudiniAssetComponent->SetHoudiniAsset( nullptr );
    }

    return FReply::Handled();
}

ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingCooking( UHoudiniAssetComponent * HoudiniAssetComponent ) const
{
    if ( HoudiniAssetComponent && HoudiniAssetComponent->bEnableCooking )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingUploadTransform(
    UHoudiniAssetComponent * HoudiniAssetComponent ) const
{
    if ( HoudiniAssetComponent && HoudiniAssetComponent->bUploadTransformsToHoudiniEngine )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingTransformCooking(
    UHoudiniAssetComponent * HoudiniAssetComponent ) const
{
    if ( HoudiniAssetComponent && HoudiniAssetComponent->bTransformChangeTriggersCooks )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingUseHoudiniMaterials(
    UHoudiniAssetComponent * HoudiniAssetComponent ) const
{
    if ( HoudiniAssetComponent && HoudiniAssetComponent->bUseHoudiniMaterials )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

ECheckBoxState
FHoudiniAssetComponentDetails::IsCheckedComponentSettingCookingTriggersDownstreamCooks(
    UHoudiniAssetComponent * HoudiniAssetComponent ) const
{
    if ( HoudiniAssetComponent && HoudiniAssetComponent->bCookingTriggersDownstreamCooks )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingCooking(
    ECheckBoxState NewState,
    UHoudiniAssetComponent * HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->bEnableCooking = ( NewState == ECheckBoxState::Checked );
}

void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingUploadTransform(
    ECheckBoxState NewState,
    UHoudiniAssetComponent * HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->bUploadTransformsToHoudiniEngine = ( NewState == ECheckBoxState::Checked );
}

void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingTransformCooking(
    ECheckBoxState NewState,
    UHoudiniAssetComponent * HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->bTransformChangeTriggersCooks = ( NewState == ECheckBoxState::Checked );
}

void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingUseHoudiniMaterials(
    ECheckBoxState NewState,
    UHoudiniAssetComponent * HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->bUseHoudiniMaterials = ( NewState == ECheckBoxState::Checked );
}

void
FHoudiniAssetComponentDetails::CheckStateChangedComponentSettingCookingTriggersDownstreamCooks(
    ECheckBoxState NewState,
    UHoudiniAssetComponent* HoudiniAssetComponent )
{
    if ( HoudiniAssetComponent )
        HoudiniAssetComponent->bCookingTriggersDownstreamCooks = ( NewState == ECheckBoxState::Checked );
}
