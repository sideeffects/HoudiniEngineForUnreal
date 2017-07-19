/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
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

#include "HoudiniApi.h"
#include "HoudiniAssetInput.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniEngine.h"
#include "HoudiniAssetActor.h"
#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniEngineString.h"
#include "HoudiniLandscapeUtils.h"
#include "Components/SplineComponent.h"
#include "Engine/Selection.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerPublicTypes.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#if WITH_EDITOR
    #include "UnrealEdGlobals.h"
    #include "Editor/UnrealEdEngine.h"
#endif

#define LOCTEXT_NAMESPACE HOUDINI_MODULE_RUNTIME

void
FHoudiniAssetInputOutlinerMesh::Serialize( FArchive & Ar )
{
    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    HoudiniAssetParameterVersion = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
    Ar << HoudiniAssetParameterVersion;

    Ar << ActorPtr;

    if ( HoudiniAssetParameterVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_ACTOR_ONLY )
    {
        Ar << StaticMeshComponent;
        Ar << StaticMesh;
    }

    Ar << ActorTransform;

    Ar << AssetId;
    if ( Ar.IsLoading() && !Ar.IsTransacting() )
        AssetId = -1;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_UNREAL_SPLINE 
        && HoudiniAssetParameterVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_ACTOR_ONLY )
    {
        Ar << SplineComponent;
        Ar << NumberOfSplineControlPoints;
        Ar << SplineLength;
        Ar << SplineResolution;
        Ar << ComponentTransform;
    }

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_ADDED_KEEP_TRANSFORM )
        Ar << KeepWorldTransform;
}

void 
FHoudiniAssetInputOutlinerMesh::RebuildSplineTransformsArrayIfNeeded()
{
    // Rebuilding the SplineTransform array after reloading the asset
    // This is required to properly detect Transform changes after loading the asset.

    // We need an Unreal spline
    if ( !SplineComponent )
        return;

    // If those are different, the input component has changed
    if ( NumberOfSplineControlPoints != SplineComponent->GetNumberOfSplinePoints() )
        return;

    // If those are equals, there's no need to rebuild the array
    if ( SplineControlPointsTransform.Num() == SplineComponent->GetNumberOfSplinePoints() )
        return;

    SplineControlPointsTransform.SetNumUninitialized(SplineComponent->GetNumberOfSplinePoints());
    for ( int32 n = 0; n < SplineControlPointsTransform.Num(); n++ )
        SplineControlPointsTransform[n] = SplineComponent->GetTransformAtSplinePoint( n, ESplineCoordinateSpace::Local, true );
}

bool
FHoudiniAssetInputOutlinerMesh::HasSplineComponentChanged(float fCurrentSplineResolution) const
{
    if ( !SplineComponent )
        return false;

    // Total length of the spline has changed ?
    if ( SplineComponent->GetSplineLength() != SplineLength )
        return true;

    // Number of CVs has changed ?
    if ( NumberOfSplineControlPoints != SplineComponent->GetNumberOfSplinePoints() )
        return true;

    if ( SplineControlPointsTransform.Num() != SplineComponent->GetNumberOfSplinePoints() )
        return true;

    // Current Spline resolution has changed?
    if ( fCurrentSplineResolution == -1.0 && SplineResolution != -1.0)
    {
        const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
        if ( HoudiniRuntimeSettings )
            fCurrentSplineResolution = HoudiniRuntimeSettings->MarshallingSplineResolution;
        else
            fCurrentSplineResolution = HAPI_UNREAL_PARAM_SPLINE_RESOLUTION_DEFAULT;
    }

    if ( SplineResolution != fCurrentSplineResolution )
        return true;

    // Has any of the CV's transform been modified?
    for ( int32 n = 0; n < SplineControlPointsTransform.Num(); n++ )
    {
        if ( !SplineControlPointsTransform[ n ].GetLocation().Equals( SplineComponent->GetLocationAtSplinePoint( n, ESplineCoordinateSpace::Local) ) )
            return true;

        if ( !SplineControlPointsTransform[ n ].GetRotation().Equals( SplineComponent->GetQuaternionAtSplinePoint( n, ESplineCoordinateSpace::World ) ) )
            return true;

        if ( !SplineControlPointsTransform[ n ].GetScale3D().Equals( SplineComponent->GetScaleAtSplinePoint( n ) ) )
            return true;
    }

    return false;
}


bool 
FHoudiniAssetInputOutlinerMesh::HasActorTransformChanged() const
{
    if ( !ActorPtr.IsValid() )
        return false;

    if ( !ActorTransform.Equals( ActorPtr->GetTransform() ) )
        return true;

    return false;
}


bool
FHoudiniAssetInputOutlinerMesh::HasComponentTransformChanged() const
{
    if ( !SplineComponent && !StaticMeshComponent )
        return false;

    if ( SplineComponent )
        return !ComponentTransform.Equals( SplineComponent->GetComponentTransform() );

    if ( StaticMeshComponent )
        return !ComponentTransform.Equals( StaticMeshComponent->GetComponentTransform() );

    return false;

}

bool
FHoudiniAssetInputOutlinerMesh::NeedsComponentUpdate() const 
{
    if ( !ActorPtr.IsValid() || ActorPtr->IsPendingKill() )
        return false;

    if ( StaticMesh && ( !StaticMesh->IsValidLowLevel() || StaticMesh->IsPendingKill() ) )
        return true;

    if ( StaticMeshComponent && ( !StaticMeshComponent->IsValidLowLevel() || StaticMeshComponent->IsPendingKill() ) )
        return true;

    if ( SplineComponent && ( !SplineComponent->IsValidLowLevel() || SplineComponent->IsPendingKill() ) )
        return true;

    if ( !StaticMeshComponent && !StaticMeshComponent && !SplineComponent )
        return true;

    return false;
}

UHoudiniAssetInput::UHoudiniAssetInput( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , InputCurve( nullptr )
    , InputAssetComponent( nullptr )
    , InputLandscapeProxy( nullptr )
    , ConnectedAssetId( -1 )
    , InputIndex( 0 )
    , ChoiceIndex( EHoudiniAssetInputType::GeometryInput )
    , UnrealSplineResolution( -1.0f )
    , OutlinerInputsNeedPostLoadInit( false )
    , HoudiniAssetInputFlagsPacked( 0u )
{
    // flags
    bLandscapeExportMaterials = true;
    bKeepWorldTransform = 2;
    bLandscapeExportAsHeightfield = true;
    bLandscapeAutoSelectComponent = true;
    bPackBeforeMerge = false;

    ChoiceStringValue = TEXT( "" );

    // Initialize the arraysc
    InputObjects.SetNumUninitialized( 1 );
    InputObjects[ 0 ] = nullptr;
    InputTransforms.SetNumUninitialized( 1 );
    InputTransforms[ 0 ] = FTransform::Identity;
    TransformUIExpanded.SetNumUninitialized( 1 );
    TransformUIExpanded[ 0 ] = false;
}

UHoudiniAssetInput::~UHoudiniAssetInput()
{}

UHoudiniAssetInput *
UHoudiniAssetInput::Create( UObject * InPrimaryObject, int32 InInputIndex, HAPI_NodeId InNodeId )
{
    UHoudiniAssetInput * HoudiniAssetInput = nullptr;

    // Get name of this input.
    HAPI_StringHandle InputStringHandle;
    if ( FHoudiniApi::GetNodeInputName(
        FHoudiniEngine::Get().GetSession(),
        InNodeId,
        InInputIndex, &InputStringHandle ) != HAPI_RESULT_SUCCESS )
    {
        return HoudiniAssetInput;
    }

    HoudiniAssetInput = NewObject< UHoudiniAssetInput >(
        InPrimaryObject,
        UHoudiniAssetInput::StaticClass(),
        NAME_None, RF_Public | RF_Transactional );

    // Set component and other information.
    HoudiniAssetInput->PrimaryObject = InPrimaryObject;
    HoudiniAssetInput->InputIndex = InInputIndex;

    // Get input string from handle.
    HoudiniAssetInput->SetNameAndLabel( InputStringHandle );

    HoudiniAssetInput->SetNodeId( InNodeId );

    // Set the default input type from the input's label
    HoudiniAssetInput->SetDefaultInputTypeFromLabel();

    // Create necessary widget resources.
    HoudiniAssetInput->CreateWidgetResources();

    return HoudiniAssetInput;
}

UHoudiniAssetInput *
UHoudiniAssetInput::Create(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo)
{
    UObject * Outer = InPrimaryObject;
    if (!Outer)
    {
        Outer = InParentParameter;
        if (!Outer)
        {
            // Must have either component or parent not null.
            check(false);
        }
    }

    UHoudiniAssetInput * HoudiniAssetInput = NewObject< UHoudiniAssetInput >(
        Outer, UHoudiniAssetInput::StaticClass(), NAME_None, RF_Public | RF_Transactional);

    // This is being used as a parameter
    HoudiniAssetInput->bIsObjectPathParameter = true;

    HoudiniAssetInput->CreateParameter(InPrimaryObject, InParentParameter, InNodeId, ParmInfo);

    HoudiniAssetInput->SetNodeId( InNodeId );

    // Set the default input type from the input's label
    HoudiniAssetInput->SetDefaultInputTypeFromLabel();

    // Create necessary widget resources.
    HoudiniAssetInput->CreateWidgetResources();

    return HoudiniAssetInput;
}

void
UHoudiniAssetInput::CreateWidgetResources()
{
    ChoiceStringValue = TEXT( "" );
    StringChoiceLabels.Empty();

    {
        FString * ChoiceLabel = new FString( TEXT( "Geometry Input" ) );
        StringChoiceLabels.Add( TSharedPtr< FString >( ChoiceLabel ) );

        if ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
            ChoiceStringValue = *ChoiceLabel;
    }
    {
        FString * ChoiceLabel = new FString( TEXT( "Asset Input" ) );
        StringChoiceLabels.Add( TSharedPtr< FString >( ChoiceLabel ) );

        if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
            ChoiceStringValue = *ChoiceLabel;
    }
    {
        FString * ChoiceLabel = new FString( TEXT( "Curve Input" ) );
        StringChoiceLabels.Add( TSharedPtr< FString >( ChoiceLabel ) );

        if ( ChoiceIndex == EHoudiniAssetInputType::CurveInput )
            ChoiceStringValue = *ChoiceLabel;
    }
    {
        FString * ChoiceLabel = new FString( TEXT( "Landscape Input" ) );
        StringChoiceLabels.Add( TSharedPtr< FString >( ChoiceLabel ) );

        if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
            ChoiceStringValue = *ChoiceLabel;
    }
    {
        FString * ChoiceLabel = new FString( TEXT( "World Outliner Input" ) );
        StringChoiceLabels.Add( TSharedPtr< FString >( ChoiceLabel ) );

        if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
            ChoiceStringValue = *ChoiceLabel;
    }
}

void
UHoudiniAssetInput::DisconnectAndDestroyInputAsset()
{
    if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
    {
        if( bIsObjectPathParameter )
        {
            std::string ParamNameString = TCHAR_TO_UTF8( *GetParameterName() );

            FHoudiniApi::SetParmStringValue(
                FHoudiniEngine::Get().GetSession(), NodeId, "",
                ParmId, 0 );
        }
        if ( InputAssetComponent )
            InputAssetComponent->RemoveDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );

        InputAssetComponent = nullptr;
        bInputAssetConnectedInHoudini = false;
        ConnectedAssetId = -1;
    }
    else
    {
        HAPI_NodeId HostAssetId = GetAssetId();
        if( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) && FHoudiniEngineUtils::IsValidAssetId( HostAssetId ) )
            FHoudiniEngineUtils::HapiDisconnectAsset( HostAssetId, InputIndex );

        if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
        {
            // World Input Actors' Meshes need to have their corresponding Input Assets destroyed too.
            for ( int32 n = 0; n < InputOutlinerMeshArray.Num(); n++ )
            {
                if ( FHoudiniEngineUtils::IsValidAssetId( InputOutlinerMeshArray[n].AssetId ) )
                {
                    FHoudiniEngineUtils::HapiDisconnectAsset( ConnectedAssetId, n );//InputOutlinerMeshArray[n].AssetId);
                    FHoudiniEngineUtils::DestroyHoudiniAsset( InputOutlinerMeshArray[n].AssetId );
                    InputOutlinerMeshArray[n].AssetId = -1;
                }
            }
        }
        else if ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
        {
            // Destroy all the geo input assets
            for ( HAPI_NodeId AssetNodeId : CreatedInputDataAssetIds )
            {
                FHoudiniEngineUtils::DestroyHoudiniAsset( AssetNodeId );
            }
            CreatedInputDataAssetIds.Empty();
        }
        else if (ChoiceIndex == EHoudiniAssetInputType::LandscapeInput)
        {
            // Destroy the landscape node
            // If the landscape was sent as a heightfield, also destroy the heightfield's input node
            FHoudiniLandscapeUtils::DestroyLandscapeAssetNode( ConnectedAssetId, CreatedInputDataAssetIds );
        }

        if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) )
        {
            FHoudiniEngineUtils::DestroyHoudiniAsset( ConnectedAssetId );
            ConnectedAssetId = -1;
        }
    }
}

bool
UHoudiniAssetInput::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo)
{
    // Inputs should never call this
    check(bIsObjectPathParameter);

    if (!Super::CreateParameter(InPrimaryObject, InParentParameter, InNodeId, ParmInfo))
        return false;

    // We can only handle node type.
    if (ParmInfo.type != HAPI_PARMTYPE_NODE)
        return false;

    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetInput::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    InputTypeComboBox.Reset();

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = LocalDetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );
    FText ParameterLabelText = FText::FromString( GetParameterLabel() );

    Row.NameWidget.Widget =
        SNew( STextBlock )
            .Text( ParameterLabelText )
            .ToolTipText( ParameterLabelText )
            .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    if ( StringChoiceLabels.Num() > 0 )
    {
	// ComboBox :  Input Type
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
        [
            SAssignNew( InputTypeComboBox, SComboBox<TSharedPtr< FString > > )
            .OptionsSource( &StringChoiceLabels )
            .InitiallySelectedItem( StringChoiceLabels[ ChoiceIndex ] )
            .OnGenerateWidget( SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateUObject(
                this, &UHoudiniAssetInput::CreateChoiceEntryWidget ) )
            .OnSelectionChanged( SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateUObject(
                this, &UHoudiniAssetInput::OnChoiceChange ) )
            [
                SNew( STextBlock )
                .Text(TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::HandleChoiceContentText ) ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];
    }

    // Checkbox : Keep World Transform
    {
        TSharedPtr< SCheckBox > CheckBoxTranformType;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
        [
            SAssignNew(CheckBoxTranformType, SCheckBox)
            .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("KeepWorldTransformCheckbox", "Keep World Transform"))
                .ToolTipText(LOCTEXT("KeepWorldTransformCheckboxTip", "Set this Input's object_merge Transform Type to INTO_THIS_OBJECT. If unchecked, it will be set to NONE."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                this, &UHoudiniAssetInput::IsCheckedKeepWorldTransform)))
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                this, &UHoudiniAssetInput::CheckStateChangedKeepWorldTransform))
        ];

        // Checkbox is read only if the input is an object-path parameter
        //if ( bIsObjectPathParameter )
        //    CheckBoxTranformType->SetEnabled(false);
    }

    // Checkbox Pack before merging
    if ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput 
        || ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        TSharedPtr< SCheckBox > CheckBoxPackBeforeMerge;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SAssignNew( CheckBoxPackBeforeMerge, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "PackBeforeMergeCheckbox", "Pack Geometry before merging" ) )
                .ToolTipText( LOCTEXT( "PackBeforeMergeCheckboxTip", "Pack each separate piece of geometry before merging them into the input." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            .IsChecked( TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                this, &UHoudiniAssetInput::IsCheckedPackBeforeMerge ) ) )
            .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                this, &UHoudiniAssetInput::CheckStateChangedPackBeforeMerge ) )
        ];
    }

    if ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
    {
        int32 Ix = 0;
        const int32 NumInputs = InputObjects.Num();
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( FText::Format( LOCTEXT( "NumArrayItemsFmt", "{0} elements" ), FText::AsNumber( NumInputs ) ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateUObject( this, &UHoudiniAssetInput::OnAddToInputObjects ), LOCTEXT( "AddInput", "Adds a Geometry Input" ), true )
            ]
            + SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateUObject( this, &UHoudiniAssetInput::OnEmptyInputObjects ), LOCTEXT( "EmptyInputs", "Removes All Inputs" ), true )
            ]
        ];

        for ( int32 Ix = 0; Ix < NumInputs; Ix++ )
        {
            UObject* InputObject = GetInputObject( Ix );
            CreateGeometryWidget( Ix, InputObject, AssetThumbnailPool, VerticalBox );
        }
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
    {
        // ActorPicker : Houdini Asset
        FMenuBuilder MenuBuilder = CreateCustomActorPickerWidget( LOCTEXT( "AssetInputSelectableActors", "Houdini Asset Actors" ), true );

        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
        [
            MenuBuilder.MakeWidget()
        ];
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::CurveInput )
    {
        // Go through all input curve parameters and build their widgets recursively.
        for ( TMap< FString, UHoudiniAssetParameter * >::TIterator
            IterParams( InputCurveParameters ); IterParams; ++IterParams )
        {
            UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
            if ( HoudiniAssetParameter )
                HoudiniAssetParameter->CreateWidget( VerticalBox );
        }
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
    {
        // ActorPicker : Landscape
        FMenuBuilder MenuBuilder = CreateCustomActorPickerWidget( LOCTEXT( "LandscapeInputSelectableActors", "Landscapes" ), true );
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
        [
            MenuBuilder.MakeWidget()
        ];

        // Checkboxes : Export Landscape As
        //		Heightfield Mesh Points
        {
            // Label
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("LandscapeExportAs", "Export Landscape As"))
                .ToolTipText(LOCTEXT("LandscapeExportAsTooltip", "Choose the type of data you want the landscape to be exported to:\n * Heightfield\n * Mesh\n * Points"))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ];

            //
            TSharedPtr<SUniformGridPanel> ButtonOptionsPanel;
            VerticalBox->AddSlot().Padding(5, 2, 5, 2).AutoHeight()
            [
                SAssignNew( ButtonOptionsPanel, SUniformGridPanel )
            ];

            // Heightfield
            FText HeightfieldTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heighfield.");
            ButtonOptionsPanel->AddSlot(0, 0)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportAsHeightfield ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportAsHeightfield ) )
                .ToolTipText( HeightfieldTooltip )
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush("ClassIcon.LandscapeComponent"))
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(STextBlock)
                        .Text( LOCTEXT( "LandscapeExportAsHeightfieldCheckbox", "Heightfield" ) )
                        .ToolTipText( HeightfieldTooltip )
                        .Font( IDetailLayoutBuilder::GetDetailFont() )
                    ]
                ]
            ];

            // Mesh
            FText MeshTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heighfield.");
            ButtonOptionsPanel->AddSlot(1, 0)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportAsMesh ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportAsMesh ) )
                .ToolTipText( MeshTooltip )
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush( "ClassIcon.StaticMeshComponent" ) )
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew( STextBlock )
                        .Text( LOCTEXT( "LandscapeExportAsMeshCheckbox", "Mesh" ) )
                        .ToolTipText( MeshTooltip )
                        .Font( IDetailLayoutBuilder::GetDetailFont() )
                    ]
                ]
            ];

            // Points
            FText PointsTooltip = LOCTEXT( "LandscapeExportAsPointsTooltip", "If enabled, the landscape will be exported to Houdini as points." );
            ButtonOptionsPanel->AddSlot(2, 0)
            [
                SNew(SCheckBox)
                .Style(FEditorStyle::Get(), "Property.ToggleButton.End")
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportAsPoints ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportAsPoints ) )
                .ToolTipText( PointsTooltip )
                [
                     SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush("Mobility.Static"))
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Padding(2, 2)
                    [
                        SNew(STextBlock)
                        .Text( LOCTEXT( "LandscapeExportAsPointsCheckbox", "Points" ) )
                        .ToolTipText( PointsTooltip )
                        .Font( IDetailLayoutBuilder::GetDetailFont() )
                    ]
                ]
            ];
        }

        // CheckBox : Export selected components only
        {
            TSharedPtr< SCheckBox > CheckBoxExportSelected;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportSelected, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeSelectedCheckbox", "Export Selected Landscape Components Only" ) )
                    .ToolTipText( LOCTEXT( "LandscapeSelectedTooltip", "If enabled, only the selected Landscape Components will be exported." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont") ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::IsCheckedExportOnlySelected) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportOnlySelected ) )
            ];
        }

        // Checkboxes auto select components
        {
            TSharedPtr< SCheckBox > CheckBoxAutoSelectComponents;
            VerticalBox->AddSlot().Padding(10, 2, 5, 2).AutoHeight()
            [
                SAssignNew( CheckBoxAutoSelectComponents, SCheckBox )
                .Content()
                [
                    SNew(STextBlock)
                    .Text( LOCTEXT( "AutoSelectComponentCheckbox", "Auto-select component in asset bounds" ) )
                    .ToolTipText( LOCTEXT( "AutoSelectComponentCheckboxTooltip", "If enabled, when no Landscape components are curremtly selected, the one within the asset's bounding box will be exported." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont") ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedAutoSelectLandscape ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedAutoSelectLandscape) )
            ];

            // Enable only when exporting selection
            // or when exporting heighfield (for now)
            if ( !bLandscapeExportSelectionOnly )
                CheckBoxAutoSelectComponents->SetEnabled( false );
        }

        // Checkbox : Export materials
        {
            TSharedPtr< SCheckBox > CheckBoxExportMaterials;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportMaterials, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeMaterialsCheckbox", "Export Landscape Materials" ) )
                    .ToolTipText( LOCTEXT( "LandscapeMaterialsTooltip", "If enabled, the landscape materials will be exported with it." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportMaterials ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportMaterials ) )
            ];

            // Disable when exporting heightfields
            if ( bLandscapeExportAsHeightfield )
                CheckBoxExportMaterials->SetEnabled( false );
        }

        // Checkbox : Export Tile UVs
        {
            TSharedPtr< SCheckBox > CheckBoxExportTileUVs;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportTileUVs, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeTileUVsCheckbox", "Export Landscape Tile UVs" ) )
                    .ToolTipText( LOCTEXT( "LandscapeTileUVsTooltip", "If enabled, UVs will be exported separately for each Landscape tile." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportTileUVs ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportTileUVs ) )
            ];

            // Disable when exporting heightfields
            if ( bLandscapeExportAsHeightfield )
                CheckBoxExportTileUVs->SetEnabled( false );
        }

        // Checkbox : Export normalized UVs
        {
            TSharedPtr< SCheckBox > CheckBoxExportNormalizedUVs;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportNormalizedUVs, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs" ) )
                    .ToolTipText( LOCTEXT( "LandscapeNormalizedUVsTooltip", "If enabled, landscape UVs will be exported in [0, 1]." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::IsCheckedExportNormalizedUVs ) ) )
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportNormalizedUVs ) )
            ];

            // Disable when exporting heightfields
            if ( bLandscapeExportAsHeightfield )
                CheckBoxExportNormalizedUVs->SetEnabled( false );
        }

        // Checkbox : Export lighting
        {
            TSharedPtr< SCheckBox > CheckBoxExportLighting;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportLighting, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeLightingCheckbox", "Export Landscape Lighting" ) )
                    .ToolTipText( LOCTEXT( "LandscapeLightingTooltip", "If enabled, lightmap information will be exported with the landscape." ) )
                    .Font(FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportLighting ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportLighting ) )
            ];

            // Disable when exporting heightfields
            if ( bLandscapeExportAsHeightfield )
                CheckBoxExportLighting->SetEnabled( false );
        }

        // Checkbox : Export landscape curves
        {
            TSharedPtr< SCheckBox > CheckBoxExportCurves;
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( CheckBoxExportCurves, SCheckBox )
                .Content()
                [
                    SNew( STextBlock )
                    .Text( LOCTEXT( "LandscapeCurvesCheckbox", "Export Landscape Curves" ) )
                    .ToolTipText( LOCTEXT( "LandscapeCurvesTooltip", "If enabled, Landscape curves will be exported." ) )
                    .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
                ]
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    this, &UHoudiniAssetInput::IsCheckedExportCurves ) ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChangedExportCurves ) )
            ];

            // Disable curves until we have them implemented.
            if ( CheckBoxExportCurves.IsValid() )
                CheckBoxExportCurves->SetEnabled( false );
        }

        // Button : Recommit
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1, 2, 4, 2 )
            [
                SNew( SButton )
                .VAlign( VAlign_Center )
                .HAlign( HAlign_Center )
                .Text( LOCTEXT( "LandscapeInputRecommit", "Recommit Landscape" ) )
                .ToolTipText( LOCTEXT( "LandscapeInputRecommitTooltip", "Recommits the Landscape to Houdini." ) )
                .OnClicked( FOnClicked::CreateUObject( this, &UHoudiniAssetInput::OnButtonClickRecommit ) )
            ]
        ];
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        // Button : Start Selection / Use current selection + refresh
        {
            TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
            FPropertyEditorModule & PropertyModule =
                FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

            // Locate the details panel.
            FName DetailsPanelName = "LevelEditorSelectionDetails";
            TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

            auto ButtonLabel = LOCTEXT( "WorldInputStartSelection", "Start Selection (Lock Details Panel)" );
            if ( DetailsView->IsLocked() )
                ButtonLabel = LOCTEXT( "WorldInputUseCurrentSelection", "Use Current Selection (Unlock Details Panel)" );

            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                SAssignNew( HorizontalBox, SHorizontalBox )
                + SHorizontalBox::Slot()
                [
                    SNew( SButton )
                    .VAlign( VAlign_Center )
                    .HAlign( HAlign_Center )
                    .Text( ButtonLabel )
                    .OnClicked( FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnButtonClickSelectActors ) )
                ]
            ];
        }

        // ActorPicker : World Outliner
        {
            FMenuBuilder MenuBuilder = CreateCustomActorPickerWidget( LOCTEXT( "WorldInputSelectedActors", "Currently Selected Actors" ), false );
            
            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
            [
                MenuBuilder.MakeWidget()
            ];
        }

        {
            // Spline Resolution
            TSharedPtr< SNumericEntryBox< float > > NumericEntryBox;
            int32 Idx = 0;

            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("SplineRes", "Unreal Spline Resolution"))
                    .ToolTipText(LOCTEXT("SplineResTooltip", "Resolution used when marshalling the Unreal Splines to HoudiniEngine.\n(step in cm betweem control points)\nSet this to 0 to only export the control points."))
                    .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
                + SHorizontalBox::Slot()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SNumericEntryBox< float >)
                    .AllowSpin(true)

                    .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

                    .MinValue(-1.0f)
                    .MaxValue(1000.0f)
                    .MinSliderValue(0.0f)
                    .MaxSliderValue(1000.0f)

                    .Value(TAttribute< TOptional< float > >::Create(TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetSplineResolutionValue) ) )
                    .OnValueChanged(SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
                        this, &UHoudiniAssetInput::SetSplineResolutionValue) )
                    .IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::IsSplineResolutionEnabled)))

                    .SliderExponent(1.0f)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .ToolTipText(LOCTEXT("SplineResToDefault", "Reset to default value."))
                    .ButtonStyle(FEditorStyle::Get(), "NoBorder")
                    .ContentPadding(0)
                    .Visibility(EVisibility::Visible)
                    .OnClicked(FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnResetSplineResolutionClicked))
                    [
                        SNew(SImage)
                        .Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
                    ]
                ]
            ];
        }
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

void 
UHoudiniAssetInput::CreateGeometryWidget( 
    int32 AtIndex, UObject* InputObject,
    TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool, TSharedRef<SVerticalBox> VerticalBox )
{
    // Create thumbnail for this static mesh.
    TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
        new FAssetThumbnail( InputObject, 64, 64, AssetThumbnailPool ) );

    TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
    // Drop Target: Static Mesh
    VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SAssetDropTarget )
            .OnIsAssetAcceptableForDrop( SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda(
                []( const UObject* InObject ) {
                    return InObject && InObject->IsA< UStaticMesh >();
            } ) )
            .OnAssetDropped( SAssetDropTarget::FOnAssetDropped::CreateUObject(
                this, &UHoudiniAssetInput::OnStaticMeshDropped, AtIndex ) )
            [
                SAssignNew( HorizontalBox, SHorizontalBox )
            ]
        ];

    // Thumbnail : Static Mesh
    FText ParameterLabelText = FText::FromString( GetParameterLabel() );
    TSharedPtr< SBorder > StaticMeshThumbnailBorder;

    HorizontalBox->AddSlot().Padding( 0.0f, 0.0f, 2.0f, 0.0f ).AutoWidth()
    [
        SAssignNew( StaticMeshThumbnailBorder, SBorder )
        .Padding( 5.0f )
        .OnMouseDoubleClick( FPointerEventHandler::CreateUObject( this, &UHoudiniAssetInput::OnThumbnailDoubleClick, AtIndex ) )
        [
            SNew( SBox )
            .WidthOverride( 64 )
            .HeightOverride( 64 )
            .ToolTipText( ParameterLabelText )
            [
                StaticMeshThumbnail->MakeThumbnailWidget()
            ]
        ]
    ];

    StaticMeshThumbnailBorder->SetBorderImage( TAttribute< const FSlateBrush * >::Create(
        TAttribute< const FSlateBrush * >::FGetter::CreateLambda( [StaticMeshThumbnailBorder]() {
        if ( StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered() )
            return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailLight" );
        else
            return FEditorStyle::GetBrush( "PropertyEditor.AssetThumbnailShadow" );
    } ) ) );

    FText MeshNameText = FText::GetEmpty();
    if ( InputObject )
        MeshNameText = FText::FromString( InputObject->GetName() );

    // ComboBox : Static Mesh
    TSharedPtr< SComboButton > StaticMeshComboButton;

    TSharedPtr< SHorizontalBox > ButtonBox;
    HorizontalBox->AddSlot()
        .FillWidth( 1.0f )
        .Padding( 0.0f, 4.0f, 4.0f, 4.0f )
        .VAlign( VAlign_Center )
        [
            SNew( SVerticalBox )
            + SVerticalBox::Slot()
            .HAlign( HAlign_Fill )
            [
                SAssignNew( ButtonBox, SHorizontalBox )
                + SHorizontalBox::Slot()
                [
                    SAssignNew( StaticMeshComboButton, SComboButton )
                    .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                    .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
                    .ContentPadding( 2.0f )
                    .ButtonContent()
                    [
                        SNew( STextBlock )
                        .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                        .Font( FEditorStyle::GetFontStyle( FName( TEXT( "PropertyWindow.NormalFont" ) ) ) )
                        .Text( MeshNameText )
                    ]
                ]
            ]
        ];

    StaticMeshComboButton->SetOnGetMenuContent( FOnGetContent::CreateLambda(
        [this, AtIndex, StaticMeshComboButton]() {
            TArray< const UClass * > AllowedClasses;
            AllowedClasses.Add( UStaticMesh::StaticClass() );

            TArray< UFactory * > NewAssetFactories;
            return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
                FAssetData( GetInputObject( AtIndex ) ),
                true,
                AllowedClasses,
                NewAssetFactories,
                OnShouldFilterStaticMesh,
                FOnAssetSelected::CreateLambda( [this, AtIndex, StaticMeshComboButton]( const FAssetData & AssetData ) {
                    if ( StaticMeshComboButton.IsValid() )
                    {
                        StaticMeshComboButton->SetIsOpen( false );

                        UObject * Object = AssetData.GetAsset();
                        OnStaticMeshDropped( Object, AtIndex );
                    }
                } ),
                FSimpleDelegate::CreateLambda( []() {} ) );
        } ) );

    // Create tooltip.
    FFormatNamedArguments Args;
    Args.Add( TEXT( "Asset" ), MeshNameText );
    FText StaticMeshTooltip = FText::Format(
        LOCTEXT( "BrowseToSpecificAssetInContentBrowser",
            "Browse to '{Asset}' in Content Browser" ), Args );

    // Button : Browse Static Mesh
    ButtonBox->AddSlot()
        .AutoWidth()
        .Padding( 2.0f, 0.0f )
        .VAlign( VAlign_Center )
        [
            PropertyCustomizationHelpers::MakeBrowseButton(
                FSimpleDelegate::CreateUObject( this, &UHoudiniAssetInput::OnStaticMeshBrowse, AtIndex ),
                TAttribute< FText >( StaticMeshTooltip ) )
        ];

    // ButtonBox : Reset
    ButtonBox->AddSlot()
        .AutoWidth()
        .Padding( 2.0f, 0.0f )
        .VAlign( VAlign_Center )
        [
            SNew( SButton )
            .ToolTipText( LOCTEXT( "ResetToBase", "Reset to default static mesh" ) )
            .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
            .ContentPadding( 0 )
            .Visibility( EVisibility::Visible )
            .OnClicked( FOnClicked::CreateUObject( this, &UHoudiniAssetInput::OnResetStaticMeshClicked, AtIndex ) )
            [
                SNew( SImage )
                .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
            ]
        ];

    ButtonBox->AddSlot()
        .Padding( 1.0f )
        .VAlign( VAlign_Center )
        .AutoWidth()
        [
            PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
            FExecuteAction::CreateLambda( [ this, AtIndex ]() 
                {
                    FScopedTransaction Transaction(
                        TEXT( HOUDINI_MODULE_RUNTIME ),
                        LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
                        PrimaryObject );
                    Modify();
                    MarkPreChanged();
                    InputObjects.Insert( nullptr, AtIndex );
                    InputTransforms.Insert( FTransform::Identity, AtIndex );
                    TransformUIExpanded.Insert( false , AtIndex );
                    bStaticMeshChanged = true;
                    MarkChanged();
                    OnParamStateChanged();
                } 
            ),
            FExecuteAction::CreateLambda( [ this, AtIndex ]()
                {
                    if ( ensure( InputObjects.IsValidIndex( AtIndex ) && InputTransforms.IsValidIndex( AtIndex ) ) )
                    {
                        FScopedTransaction Transaction(
                            TEXT( HOUDINI_MODULE_RUNTIME ),
                            LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
                            PrimaryObject );
                        Modify();
                        MarkPreChanged();
                        InputObjects.RemoveAt( AtIndex );
                        InputTransforms.RemoveAt( AtIndex );
                        TransformUIExpanded.RemoveAt( AtIndex );
                        bStaticMeshChanged = true;
                        MarkChanged();
                        OnParamStateChanged();
                    }
                } 
            ),
            FExecuteAction::CreateLambda( [ this, AtIndex ]()
                {
                    if ( ensure( InputObjects.IsValidIndex( AtIndex ) ) && InputTransforms.IsValidIndex( AtIndex ) )
                    {
                        FScopedTransaction Transaction(
                            TEXT( HOUDINI_MODULE_RUNTIME ),
                            LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
                            PrimaryObject );
                        Modify();
                        MarkPreChanged();
                        UObject* Dupe = InputObjects[ AtIndex ];
                        InputObjects.Insert( Dupe , AtIndex );
                        FTransform DupeTransform = InputTransforms[ AtIndex ];
                        InputTransforms.Insert( DupeTransform, AtIndex );
                        bool DupeUIExpanded = TransformUIExpanded[ AtIndex ];
                        TransformUIExpanded.Insert( DupeUIExpanded, AtIndex );
                        bStaticMeshChanged = true;
                        MarkChanged();
                        OnParamStateChanged();
                    }
                } 
            ) )
        ];


    
    {
        //TSharedPtr<SButton> ExpanderArrow;
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( ExpanderArrow, SButton )
                .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                .ClickMethod( EButtonClickMethod::MouseDown )
                    .Visibility( EVisibility::Visible )
                .OnClicked( FOnClicked::CreateUObject(this, &UHoudiniAssetInput::OnExpandInputTransform, AtIndex ) )
                [
                    SNew( SImage )
                    .Image( FEditorStyle::GetBrush( TEXT( "TreeArrow_Collapsed" ) ) )
                    .Image( TAttribute<const FSlateBrush*>::Create(
                        TAttribute<const FSlateBrush*>::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetExpanderImage, AtIndex ) ) )
                    .ColorAndOpacity( FSlateColor::UseForeground() )
                ]
            ]
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT("GeoInputTransform", "Transform Offset") )
                .ToolTipText( LOCTEXT( "GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];
    }

    // TRANSFORM 
    if ( TransformUIExpanded[ AtIndex ] )
    {
        // Position
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text( LOCTEXT("GeoInputTranslate", "T") )
                .ToolTipText( LOCTEXT( "GeoInputTranslateTooltip", "Translate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
                .X( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetPositionX, AtIndex ) ) )
                .Y( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float> >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetPositionY, AtIndex ) ) )
                .Z( TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetPositionZ, AtIndex ) ) )
                .OnXChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetPositionX, AtIndex ) )
                .OnYChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetPositionY, AtIndex ) )
                .OnZChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetPositionZ, AtIndex ) )
            ]
        ];

        // Rotation
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text( LOCTEXT("GeoInputRotate", "R") )
                .ToolTipText( LOCTEXT( "GeoInputRotateTooltip", "Rotate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SRotatorInputBox )
                .AllowSpin( true )
                .bColorAxisLabels( true )
                .Roll( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetRotationRoll, AtIndex ) ) )
                .Pitch( TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetRotationPitch, AtIndex) ) )
                .Yaw( TAttribute<TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetRotationYaw, AtIndex) ) )
                .OnRollChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetRotationRoll, AtIndex) )
                .OnPitchChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetRotationPitch, AtIndex) )
                .OnYawChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetRotationYaw, AtIndex) )
            ]
        ];

        // Scale
	VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "GeoInputScale", "S" ) )
                .ToolTipText( LOCTEXT( "GeoInputScaleTooltip", "Scale" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot().MaxWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH )
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
                .X( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetScaleX, AtIndex ) ) )
                .Y( TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float> >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetScaleY, AtIndex ) ) )
                .Z( TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::GetScaleZ, AtIndex ) ) )
                .OnXChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetScaleX, AtIndex ) )
                .OnYChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetScaleY, AtIndex ) )
                .OnZChanged( FOnFloatValueChanged::CreateUObject(
                    this, &UHoudiniAssetInput::SetScaleZ, AtIndex ) )
            ]
            /*
            + SHorizontalBox::Slot().AutoWidth()
            [
                // Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
                SNew( SCheckBox )
                .Style( FEditorStyle::Get(), "TransparentCheckBox" )
                .ToolTipText( LOCTEXT( "PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
                .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
                    this, &UHoudiniAssetInput::CheckStateChanged, AtIndex ) )
                .IsChecked( TAttribute< ECheckBoxState >::Create(
                    TAttribute<ECheckBoxState>::FGetter::CreateUObject(
                        this, &UHoudiniAssetInput::IsChecked, AtIndex ) ) )
                [
                    SNew( SImage )
                    .Image( TAttribute<const FSlateBrush*>::Create(
                        TAttribute<const FSlateBrush*>::FGetter::CreateUObject(
                            this, &UHoudiniAssetInput::GetPreserveScaleRatioImage, AtIndex ) ) )
                    .ColorAndOpacity( FSlateColor::UseForeground() )
                ]
            ]
            */
        ];
    }
}

void
UHoudiniAssetInput::PostEditUndo()
{
    Super::PostEditUndo();

    if ( InputCurve && ChoiceIndex == EHoudiniAssetInputType::CurveInput )
    {
        if( USceneComponent* RootComp = GetHoudiniAssetComponent() )
        {
            AActor* Owner = RootComp->GetOwner();
            Owner->AddOwnedComponent( InputCurve );

            InputCurve->AttachToComponent( RootComp, FAttachmentTransformRules::KeepRelativeTransform );
            InputCurve->RegisterComponent();
            InputCurve->SetVisibility( true );
        }
    }
}

// Note: This method is only used for testing
void 
UHoudiniAssetInput::ForceSetInputObject( UObject * InObject, int32 AtIndex, bool CommitChange )
{
    if( AActor* Actor = Cast<AActor>( InObject ) )
    {
        for( UActorComponent * Component : Actor->GetComponentsByClass( UStaticMeshComponent::StaticClass() ) )
        {
            UStaticMeshComponent * StaticMeshComponent = CastChecked< UStaticMeshComponent >( Component );
            if( !StaticMeshComponent )
                continue;

            UStaticMesh * StaticMesh = StaticMeshComponent->GetStaticMesh();
            if( !StaticMesh )
                continue;

            // Add the mesh to the array
            FHoudiniAssetInputOutlinerMesh OutlinerMesh;

            OutlinerMesh.ActorPtr = Actor;
            OutlinerMesh.StaticMeshComponent = StaticMeshComponent;
            OutlinerMesh.StaticMesh = StaticMesh;
            OutlinerMesh.SplineComponent = nullptr;
            OutlinerMesh.AssetId = -1;

            UpdateWorldOutlinerTransforms( OutlinerMesh );

            InputOutlinerMeshArray.Add( OutlinerMesh );
        }

        // Looking for Splines
        for( UActorComponent * Component : Actor->GetComponentsByClass( USplineComponent::StaticClass() ) )
        {
            USplineComponent * SplineComponent = CastChecked< USplineComponent >( Component );
            if( !SplineComponent )
                continue;

            // Add the spline to the array
            FHoudiniAssetInputOutlinerMesh OutlinerMesh;

            OutlinerMesh.ActorPtr = Actor;
            OutlinerMesh.StaticMeshComponent = nullptr;
            OutlinerMesh.StaticMesh = nullptr;
            OutlinerMesh.SplineComponent = SplineComponent;
            OutlinerMesh.AssetId = -1;

            UpdateWorldOutlinerTransforms( OutlinerMesh );

            InputOutlinerMeshArray.Add( OutlinerMesh );
        }
    }

    if( InputObjects.IsValidIndex( AtIndex ) )
    {
        InputObjects[ AtIndex ] = InObject;
    }
    else
    {
        InputObjects.Insert( InObject, AtIndex );
    }

    if ( !InputTransforms.IsValidIndex( AtIndex ) )
    {
        InputTransforms.Insert( FTransform::Identity, AtIndex );
    }

    if ( !TransformUIExpanded.IsValidIndex( AtIndex ) )
    {
        TransformUIExpanded.Insert( false, AtIndex );
    }

    if( CommitChange )
    {
        if( InputOutlinerMeshArray.Num() > 0 )
            ChangeInputType( EHoudiniAssetInputType::WorldInput );
        else
            ChangeInputType( EHoudiniAssetInputType::GeometryInput );

        MarkChanged();
    }
}

// Note: This method is only used for testing
void 
UHoudiniAssetInput::ClearInputs()
{
    ChangeInputType( EHoudiniAssetInputType::GeometryInput );
    InputOutlinerMeshArray.Empty();
    InputObjects.Empty();
    InputTransforms.Empty();
    TransformUIExpanded.Empty();
    MarkChanged();
}

#endif

bool
UHoudiniAssetInput::ConnectInputNode()
{
    // Helper for connecting our input or setting the object path parameter
    if (!bIsObjectPathParameter)
    {
        // Now we can connect input node to the asset node.
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), GetAssetId(), InputIndex,
            ConnectedAssetId), false);
    }
    else
    {
        // Now we can assign the input node path to the parameter
        std::string ParamNameString = TCHAR_TO_UTF8(*GetParameterName());
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetParmNodeValue(
            FHoudiniEngine::Get().GetSession(), NodeId,
            ParamNameString.c_str(), ConnectedAssetId), false);
    }
    return true;
}

bool
UHoudiniAssetInput::UploadParameterValue()
{
    bool Success = true;

    if ( PrimaryObject == nullptr )
        return false;
    
    HAPI_NodeId HostAssetId = GetAssetId();

    switch ( ChoiceIndex )
    {
        case EHoudiniAssetInputType::GeometryInput:
        {
            if ( ! InputObjects.Num() )
            {
                // Either mesh was reset or null mesh has been assigned.
                DisconnectAndDestroyInputAsset();
            }
            else
            {
                if ( bStaticMeshChanged || bLoadedParameter )
                {
                    // Disconnect and destroy currently connected asset, if there's one.
                    DisconnectAndDestroyInputAsset();

                    // Connect input and create connected asset. Will return by reference.
                    if ( !FHoudiniEngineUtils::HapiCreateInputNodeForData( 
                        HostAssetId, InputObjects, InputTransforms,
                        ConnectedAssetId, CreatedInputDataAssetIds ) )
                    {
                        bChanged = false;
                        ConnectedAssetId = -1;
                        return false;
                    }
                    else
                    {
                        Success &= ConnectInputNode();
                    }

                    bStaticMeshChanged = false;
                }

                Success &= UpdateObjectMergeTransformType();
                Success &= UpdateObjectMergePackBeforeMerge();
            }
            break;
        }
    
        case EHoudiniAssetInputType::AssetInput:
        {
            // Process connected asset.
            if ( InputAssetComponent && FHoudiniEngineUtils::IsValidAssetId( InputAssetComponent->GetAssetId() )
                && !bInputAssetConnectedInHoudini )
            {
                ConnectInputAssetActor();
            }
            else if ( bInputAssetConnectedInHoudini && !InputAssetComponent )
            {
                DisconnectInputAssetActor();
            }
            else
            {
                bChanged = false;
                if ( InputAssetComponent )
                    return false;
            }

            break;
        }

        case EHoudiniAssetInputType::CurveInput:
        {
            // If we have no curve node, create it.
            bool bCreated = false;
            if ( !FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) )
            {
                if ( !FHoudiniEngineUtils::HapiCreateCurveNode( ConnectedAssetId ) )
                {
                    bChanged = false;
                    ConnectedAssetId = -1;
                    return false;
                }

                // Connect the node to the asset.
                Success &= ConnectInputNode();

                bCreated = true;
            }

            if ( bLoadedParameter || bCreated )
            {
                // If we just loaded or created our curve, we need to set parameters.
                for (TMap< FString, UHoudiniAssetParameter * >::TIterator
                IterParams(InputCurveParameters); IterParams; ++IterParams)
                {
                    UHoudiniAssetParameter * Parameter = IterParams.Value();
                    if (Parameter == nullptr)
                        continue;

                    // We need to update the node id for the parameters.
                    Parameter->SetNodeId(ConnectedAssetId);

                    // Upload parameter value.
                    Success &= Parameter->UploadParameterValue();
                }
            }

            if ( ConnectedAssetId != -1 && InputCurve )
            {
                // The curve node has now been created and set up, we can upload points and rotation/scale attributes
                const TArray< FTransform > & CurvePoints = InputCurve->GetCurvePoints();
                TArray<FVector> Positions;
                InputCurve->GetCurvePositions(Positions);

                TArray<FQuat> Rotations;
                InputCurve->GetCurveRotations(Rotations);

                TArray<FVector> Scales;
                InputCurve->GetCurveScales(Scales);

                if(!FHoudiniEngineUtils::HapiCreateCurveInputNodeForData(
                        HostAssetId,
                        ConnectedAssetId,
                        &Positions,
                        &Rotations,
                        &Scales,
                        nullptr))
                {
                    bChanged = false;
                    ConnectedAssetId = -1;
                    return false;
                }
            }
            
            if (bCreated && InputCurve)
            {
                // We need to check that the SplineComponent has no offset.
                // if the input was set to WorldOutliner before, it might have one
                FTransform CurveTransform = InputCurve->GetRelativeTransform();	
                if (!CurveTransform.GetLocation().IsZero())
                    InputCurve->SetRelativeLocation(FVector::ZeroVector);
            }

            Success &= UpdateObjectMergeTransformType();

            // Cook the spline node.
            if( HAPI_RESULT_SUCCESS != FHoudiniApi::CookNode( FHoudiniEngine::Get().GetSession(), ConnectedAssetId, nullptr ) )
                Success = false;

            // We need to update the curve.
            Success &= UpdateInputCurve();

            bSwitchedToCurve = false;

            break;
        }
    
        case EHoudiniAssetInputType::LandscapeInput:
        {
            if ( InputLandscapeProxy == nullptr)
            {
                // Either landscape was reset or null landscape has been assigned.
                DisconnectAndDestroyInputAsset();
            }
            else
            {
                // Disconnect and destroy currently connected asset, if there's one.
                DisconnectAndDestroyInputAsset();

                UHoudiniAssetComponent* AssetComponent = ( UHoudiniAssetComponent* )PrimaryObject;
                if ( !AssetComponent )
                    AssetComponent = InputAssetComponent;

                // We need to get the asset bounds, without this input
                FBox Bounds( ForceInitToZero );
                if ( AssetComponent )
                    Bounds = AssetComponent->GetAssetBounds( this, true );

                // Connect input and create connected asset. Will return by reference.
                if ( !FHoudiniEngineUtils::HapiCreateInputNodeForData(
                        HostAssetId, InputLandscapeProxy,
                        ConnectedAssetId, CreatedInputDataAssetIds,
                        bLandscapeExportSelectionOnly, bLandscapeExportCurves,
                        bLandscapeExportMaterials, bLandscapeExportAsMesh, bLandscapeExportLighting,
                        bLandscapeExportNormalizedUVs, bLandscapeExportTileUVs, Bounds,
                        bLandscapeExportAsHeightfield, bLandscapeAutoSelectComponent ) )
                {
                    bChanged = false;
                    ConnectedAssetId = -1;
                    return false;
                }

                // Connect the inputs and update the transform type
                Success &= ConnectInputNode();
                Success &= UpdateObjectMergeTransformType();
            }
            break;
        }

        case EHoudiniAssetInputType::WorldInput:
        {
            if ( InputOutlinerMeshArray.Num() <= 0 )
            {
                // Either mesh was reset or null mesh has been assigned.
                DisconnectAndDestroyInputAsset();
            }
            else
            {
                if ( bStaticMeshChanged || bLoadedParameter )
                {
                    // Disconnect and destroy currently connected asset, if there's one.
                    DisconnectAndDestroyInputAsset();

                    // Connect input and create connected asset. Will return by reference.
                    if ( !FHoudiniEngineUtils::HapiCreateInputNodeForData(
                        HostAssetId, InputOutlinerMeshArray,
                        ConnectedAssetId, UnrealSplineResolution ) )
                    {
                        bChanged = false;
                        ConnectedAssetId = -1;
                        return false;
                    }
                    else
                    {
                        Success &= ConnectInputNode();
                    }

                    bStaticMeshChanged = false;

                    Success &= UpdateObjectMergeTransformType();
                }

                Success &= UpdateObjectMergePackBeforeMerge();
            }
            break;
        }

        default:
        {
            check( 0 );
        }
    }

    bLoadedParameter = false;
    return Success & Super::UploadParameterValue();
}


uint32
UHoudiniAssetInput::GetDefaultTranformTypeValue() const
{
    switch (ChoiceIndex)
    {
	// NONE
	case EHoudiniAssetInputType::CurveInput:
	case EHoudiniAssetInputType::GeometryInput:	
	    return 0;	
	
	// INTO THIS OBJECT
	case EHoudiniAssetInputType::AssetInput:
	case EHoudiniAssetInputType::LandscapeInput:
	case EHoudiniAssetInputType::WorldInput:
	    return 1;
    }

    return 0;
}


UObject* 
UHoudiniAssetInput::GetInputObject( int32 AtIndex ) const
{
    return InputObjects.IsValidIndex( AtIndex ) ? InputObjects[ AtIndex ] : nullptr;
}

FTransform
UHoudiniAssetInput::GetInputTransform( int32 AtIndex ) const
{
    return InputTransforms.IsValidIndex( AtIndex ) ? InputTransforms[ AtIndex ] : FTransform::Identity;
}

UHoudiniAssetComponent* 
UHoudiniAssetInput::GetHoudiniAssetComponent()
{
    return Cast<UHoudiniAssetComponent>( PrimaryObject );
}

const UHoudiniAssetComponent*
UHoudiniAssetInput::GetHoudiniAssetComponent() const
{
    return Cast<const UHoudiniAssetComponent>( PrimaryObject );
}

HAPI_NodeId 
UHoudiniAssetInput::GetAssetId() const
{
    return NodeId;
}

bool
UHoudiniAssetInput::UpdateObjectMergeTransformType()
{
    if ( PrimaryObject == nullptr )
        return false;

    uint32 nTransformType = -1;
    if ( bKeepWorldTransform == 2 )
        nTransformType = GetDefaultTranformTypeValue();
    else if ( bKeepWorldTransform )
        nTransformType = 1; 
    else
        nTransformType = 0;

    // Get the Input node ID from the host ID
    HAPI_NodeId InputNodeId = -1;
    HAPI_NodeId HostAssetId = GetAssetId();

    bool bSuccess = false;
    const std::string sXformType = "xformtype";

    if ( bIsObjectPathParameter )
    {
        // Directly change the Parameter xformtype
        if ( HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
            FHoudiniEngine::Get().GetSession(),
            NodeId, sXformType.c_str(), 0, nTransformType ) )
            bSuccess = true;
    }
    else
    {
        // Query the object merge's node ID via the input
        if ( HAPI_RESULT_SUCCESS == FHoudiniApi::QueryNodeInput(
            FHoudiniEngine::Get().GetSession(),
            HostAssetId, InputIndex, &InputNodeId ) )
        {
            // Change Parameter xformtype
            if ( HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
                FHoudiniEngine::Get().GetSession(), 
                InputNodeId, sXformType.c_str(), 0, nTransformType ) )
                bSuccess = true;
        }
    }

    // Going through each input asset plugged in the geometry input,
    // or through each input asset select in a world input.    
    int32 NumberOfInputObjects = 0;
    if ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
        NumberOfInputObjects = InputObjects.Num();
    else if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
        NumberOfInputObjects = InputOutlinerMeshArray.Num();
    
    // We also need to modify the transform types of the merge node's inputs
    for ( int32 n = 0; n < NumberOfInputObjects; n++ )
    {
        // Get the Input node ID from the host ID
        InputNodeId = -1;
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, n, &InputNodeId ) )
            continue;

        if ( InputNodeId == -1 )
            continue;

        // Change Parameter xformtype
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
            FHoudiniEngine::Get().GetSession(), InputNodeId,
            sXformType.c_str(), 0, nTransformType ) )
            bSuccess =  false;
    }

    return bSuccess;
}


bool
UHoudiniAssetInput::UpdateObjectMergePackBeforeMerge()
{
    if ( PrimaryObject == nullptr )
        return false;

    uint32 nPackValue = bPackBeforeMerge ? 1 : 0;

    // Get the Input node ID from the host ID
    HAPI_NodeId InputNodeId = -1;
    HAPI_NodeId HostAssetId = GetAssetId();

    bool bSuccess = true;
    const std::string sPack = "pack";

    // Going through each input asset plugged in the geometry input,
    // or through each input asset select in a world input.    
    int32 NumberOfInputObjects = 0;
    if (ChoiceIndex == EHoudiniAssetInputType::GeometryInput)
        NumberOfInputObjects = InputObjects.Num();
    else if (ChoiceIndex == EHoudiniAssetInputType::WorldInput)
        NumberOfInputObjects = InputOutlinerMeshArray.Num();

    // We also need to modify the transform types of the merge node's inputs
    for ( int n = 0; n < NumberOfInputObjects; n++ )
    {
        // Get the Input node ID from the host ID
        InputNodeId = -1;
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
            FHoudiniEngine::Get().GetSession(),
            ConnectedAssetId, n, &InputNodeId ) )
            continue;

        if ( InputNodeId == -1 )
            continue;

        // Change Parameter xformtype
        if ( HAPI_RESULT_SUCCESS != FHoudiniApi::SetParmIntValue(
            FHoudiniEngine::Get().GetSession(), InputNodeId,
            sPack.c_str(), 0, nPackValue ) )
            bSuccess = false;
    }

    return bSuccess;
}

void
UHoudiniAssetInput::BeginDestroy()
{
    Super::BeginDestroy();

    // Destroy anything curve related.
    DestroyInputCurve();

    // Disconnect and destroy the asset we may have connected.
    DisconnectAndDestroyInputAsset();
}

void
UHoudiniAssetInput::PostLoad()
{
    Super::PostLoad();

    // Generate widget related resources.
    CreateWidgetResources();

    // Patch input curve parameter links.
    for ( TMap< FString, UHoudiniAssetParameter * >::TIterator IterParams( InputCurveParameters ); IterParams; ++IterParams )
    {
        FString ParameterKey = IterParams.Key();
        UHoudiniAssetParameter * Parameter = IterParams.Value();

        if ( Parameter )
        {
            Parameter->SetHoudiniAssetComponent( nullptr );
            Parameter->SetParentParameter( this );
        }
    }
  
    if ( InputCurve )
    {
        if (ChoiceIndex == EHoudiniAssetInputType::CurveInput)
        {
            // Set input callback object for this curve.
            InputCurve->SetHoudiniAssetInput(this);
            if( USceneComponent* RootComp = GetHoudiniAssetComponent() )
            {
                InputCurve->AttachToComponent( RootComp, FAttachmentTransformRules::KeepRelativeTransform );
            }
        }
        else
        {
            // Manually destroying the "ghost" curve
            InputCurve->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
            InputCurve->UnregisterComponent();
            InputCurve->DestroyComponent();
            InputCurve = nullptr;
        }
    }

    if (InputOutlinerMeshArray.Num() > 0)
    {
        // Proper initialization of the outliner inputs is delayed to the first WorldTick,
        // As some of the Actors' components might not be preoperly initalized yet
        OutlinerInputsNeedPostLoadInit = true;

#if WITH_EDITOR
        StartWorldOutlinerTicking();
#endif
    }

    // Also set the expanded ui
    TransformUIExpanded.SetNumZeroed( InputObjects.Num() );
}

void
UHoudiniAssetInput::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    // Serialize current choice selection.
    SerializeEnumeration< EHoudiniAssetInputType::Enum >( Ar, ChoiceIndex );
    Ar << ChoiceStringValue;

    // We need these temporary variables for undo state tracking.
    bool bLocalInputAssetConnectedInHoudini = bInputAssetConnectedInHoudini;
    UHoudiniAssetComponent * LocalInputAssetComponent = InputAssetComponent;

    Ar << HoudiniAssetInputFlagsPacked;

    // Serialize input index.
    Ar << InputIndex;

    // Serialize input objects (if it's assigned).
    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_MULTI_GEO_INPUT)
    {
        Ar << InputObjects;
    }
    else
    {
        UObject* InputObject = nullptr;
        Ar << InputObject;
        InputObjects.Empty();
        InputObjects.Add( InputObject );
    }

    // Serialize input asset.
    Ar << InputAssetComponent;

    // Serialize curve and curve parameters (if we have those).
    Ar << InputCurve;
    Ar << InputCurveParameters;

    // Serialize landscape used for input.
    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_LANDSCAPE_INPUT )
        Ar << InputLandscapeProxy;

    // Serialize world outliner inputs.
    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_WORLD_OUTLINER_INPUT )
    {
        Ar << InputOutlinerMeshArray;
    }

    // Create necessary widget resources.
    if ( Ar.IsLoading() )
    {
        bLoadedParameter = true;

        if ( Ar.IsTransacting() )
        {
            bInputAssetConnectedInHoudini = bLocalInputAssetConnectedInHoudini;

            if ( LocalInputAssetComponent != InputAssetComponent )
            {
                if ( InputAssetComponent )
                    bInputAssetConnectedInHoudini = false;

                if ( LocalInputAssetComponent )
                    LocalInputAssetComponent->RemoveDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );
            }
        }
        else
        {
            // If we're loading for real for the first time we need to reset this
            // flag so we can reconnect when we get our parameters uploaded.
            bInputAssetConnectedInHoudini = false;
        }
    }

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_UNREAL_SPLINE_RESOLUTION_PER_INPUT )
        Ar << UnrealSplineResolution;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_GEOMETRY_INPUT_TRANSFORMS )
    {
        Ar << InputTransforms;
    }
    else
    {
        InputTransforms.SetNum( InputObjects.Num() );
        for( int32 n = 0; n < InputTransforms.Num(); n++ )
            InputTransforms[ n ] = FTransform::Identity;
    }
}

void
UHoudiniAssetInput::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetInput * HoudiniAssetInput = Cast< UHoudiniAssetInput >( InThis );
    if ( HoudiniAssetInput )
    {
        // Add reference to held geometry object.
        if ( HoudiniAssetInput->InputObjects.Num() )
            Collector.AddReferencedObjects( HoudiniAssetInput->InputObjects, InThis );

        // Add reference to held input asset component, if we have one.
        if ( HoudiniAssetInput->InputAssetComponent )
            Collector.AddReferencedObject( HoudiniAssetInput->InputAssetComponent, InThis );

        // Add reference to held curve object.
        if ( HoudiniAssetInput->InputCurve )
            Collector.AddReferencedObject( HoudiniAssetInput->InputCurve, InThis );

        // Add reference to held landscape.
        if ( HoudiniAssetInput->InputLandscapeProxy )
            Collector.AddReferencedObject( HoudiniAssetInput->InputLandscapeProxy, InThis );

        // Add reference for the WorldInputs' Actors
        for ( auto & OutlinerInput : HoudiniAssetInput->InputOutlinerMeshArray )
        {
            // Add the outliner input's actor
            AActor * OutlinerInputActor = OutlinerInput.ActorPtr.IsValid() ? OutlinerInput.ActorPtr.Get() : nullptr;
            if ( !OutlinerInputActor )
                continue;

            Collector.AddReferencedObject( OutlinerInputActor, InThis );
        }

        // Add references for all curve input parameters.
        for ( TMap< FString, UHoudiniAssetParameter * >::TIterator IterParams( HoudiniAssetInput->InputCurveParameters );
            IterParams; ++IterParams )
        {
            UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
            if ( HoudiniAssetParameter )
                Collector.AddReferencedObject( HoudiniAssetParameter, InThis );
        }
    }

    // Call base implementation.
    Super::AddReferencedObjects( InThis, Collector );
}

void
UHoudiniAssetInput::ClearInputCurveParameters()
{
    for( TMap< FString, UHoudiniAssetParameter * >::TIterator IterParams( InputCurveParameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if ( HoudiniAssetParameter )
            HoudiniAssetParameter->ConditionalBeginDestroy();
    }

    InputCurveParameters.Empty();
}

void
UHoudiniAssetInput::DisconnectInputCurve()
{
    // If we have spline, delete it.
    if ( InputCurve )
    {
        InputCurve->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
        InputCurve->UnregisterComponent();
    }
}

void
UHoudiniAssetInput::DestroyInputCurve()
{
    // If we have spline, delete it.
    if ( InputCurve )
    {
        InputCurve->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
        InputCurve->UnregisterComponent();
        InputCurve->DestroyComponent();

        InputCurve = nullptr;
    }

    ClearInputCurveParameters();
}

#if WITH_EDITOR

void
UHoudiniAssetInput::OnStaticMeshDropped( UObject * InObject, int32 AtIndex )
{
    UObject* InputObject = GetInputObject( AtIndex );
    if ( InObject != InputObject )
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
            PrimaryObject );
        Modify();

        MarkPreChanged();
        if ( InputObjects.IsValidIndex( AtIndex ) )
        {
            InputObjects[ AtIndex ] = InObject;
        }
        else
        {
            check( AtIndex == 0 );
            InputObjects.Add( InObject );
        }

        if ( !InputTransforms.IsValidIndex( AtIndex ) )
            InputTransforms.Add( FTransform::Identity );

        if ( !TransformUIExpanded.IsValidIndex( AtIndex ) )
            TransformUIExpanded.Add( false );

        bStaticMeshChanged = true;
        MarkChanged();

        OnParamStateChanged();
    }
}

FReply
UHoudiniAssetInput::OnThumbnailDoubleClick( const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent, int32 AtIndex )
{
    UObject* InputObject = GetInputObject( AtIndex );
    if ( InputObject && InputObject->IsA( UStaticMesh::StaticClass() ) && GEditor )
        GEditor->EditObject( InputObject );

    return FReply::Handled();
}

TSharedRef< SWidget >
UHoudiniAssetInput::CreateChoiceEntryWidget( TSharedPtr< FString > ChoiceEntry )
{
    FText ChoiceEntryText = FText::FromString( *ChoiceEntry );

    return SNew( STextBlock )
        .Text( ChoiceEntryText )
        .ToolTipText( ChoiceEntryText )
        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) );
}

void UHoudiniAssetInput::OnStaticMeshBrowse(int32 AtIndex)
{
    UObject* InputObject = GetInputObject( AtIndex );
    if ( GEditor && InputObject )
    {
        TArray< UObject * > Objects;
        Objects.Add( InputObject );
        GEditor->SyncBrowserToObjects( Objects );
    }
}

FReply
UHoudiniAssetInput::OnResetStaticMeshClicked( int32 AtIndex )
{
    OnStaticMeshDropped( nullptr, AtIndex );
    return FReply::Handled();
}


void
UHoudiniAssetInput::OnChoiceChange( TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType )
{
    if ( !NewChoice.IsValid() )
        return;

    ChoiceStringValue = *( NewChoice.Get() );

    // We need to match selection based on label.
    bool bLocalChanged = false;
    int32 ActiveLabel = 0;

    for ( int32 LabelIdx = 0; LabelIdx < StringChoiceLabels.Num(); ++LabelIdx )
    {
        FString * ChoiceLabel = StringChoiceLabels[ LabelIdx ].Get();

        if ( ChoiceLabel && ChoiceLabel->Equals( ChoiceStringValue ) )
        {
            bLocalChanged = true;
            ActiveLabel = LabelIdx;
            break;
        }
    }

    if ( !bLocalChanged )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Type Change" ),
        PrimaryObject );
    Modify();

    // Switch mode.
    EHoudiniAssetInputType::Enum newChoice = static_cast<EHoudiniAssetInputType::Enum>(ActiveLabel);
    ChangeInputType(newChoice);
}

bool
UHoudiniAssetInput::ChangeInputType(const EHoudiniAssetInputType::Enum& newType)
{
    switch (ChoiceIndex)
    {
        case EHoudiniAssetInputType::GeometryInput:
        {
            // We are switching away from geometry input.
            break;
        }

        case EHoudiniAssetInputType::AssetInput:
        {
            // We are switching away from asset input.  WIll be handled in DisconnectAndDestroyInputAsset
            break;
        }

        case EHoudiniAssetInputType::CurveInput:
        {
            // We are switching away from curve input.
            DisconnectInputCurve();
            break;
        }

        case EHoudiniAssetInputType::LandscapeInput:
        {
            /*
            // We are switching away from landscape input.
            if ( newType != ChoiceIndex )
            {
                // Reset selected landscape.
                InputLandscapeProxy = nullptr;
            }
            */
            break;
        }

        case EHoudiniAssetInputType::WorldInput:
        {
            // We are switching away from World Outliner input.

            // Stop monitoring the Actors for transform changes.
            StopWorldOutlinerTicking();

            break;
        }

        default:
        {
            // Unhandled new input type?
            check(0);
            break;
        }
    }

    // Disconnect currently connected asset.
    DisconnectAndDestroyInputAsset();

    // Make sure we'll fully update the editor properties
    if ( ChoiceIndex != newType && InputAssetComponent )
        InputAssetComponent->bEditorPropertiesNeedFullUpdate = true;

    // Switch mode.
    ChoiceIndex = newType;

    // Switch mode.
    switch (newType)
    {
        case EHoudiniAssetInputType::GeometryInput:
        {
            // We are switching to geometry input.
            if ( InputObjects.Num() )
                bStaticMeshChanged = true;
            break;
        }

        case EHoudiniAssetInputType::AssetInput:
        {
            // We are switching to asset input.
            ConnectInputAssetActor();
            break;
        }

        case EHoudiniAssetInputType::CurveInput:
        {
            // We are switching to curve input.

            // Create new spline component if necessary.
            if( USceneComponent* RootComp = GetHoudiniAssetComponent() )
            {
                if( !InputCurve )
                {
                    InputCurve = NewObject< UHoudiniSplineComponent >(
                        RootComp->GetOwner(), UHoudiniSplineComponent::StaticClass(),
                        NAME_None, RF_Public | RF_Transactional );
                }
                // Attach or re-attach curve component to asset.
                InputCurve->AttachToComponent( RootComp, FAttachmentTransformRules::KeepRelativeTransform );
                InputCurve->RegisterComponent();
                InputCurve->SetVisibility( true );
                InputCurve->SetHoudiniAssetInput( this );

                bSwitchedToCurve = true;
            }
            break;
        }

        case EHoudiniAssetInputType::LandscapeInput:
        {
            // We are switching to Landscape input.
            break;
        }

        case EHoudiniAssetInputType::WorldInput:
        {
            // We are switching to World Outliner input.

            // Start monitoring the Actors for transform changes.
            StartWorldOutlinerTicking();

            // Force recook and reconnect of the input assets.
            HAPI_NodeId HostAssetId = GetAssetId();
            if (FHoudiniEngineUtils::HapiCreateInputNodeForData(
                HostAssetId, InputOutlinerMeshArray,
                ConnectedAssetId, UnrealSplineResolution))
            {
                ConnectInputNode();
            }

            break;
        }

        default:
        {
            // Unhandled new input type?
            check(0);
            break;
        }
    }

    // If we have input object and geometry asset, we need to connect it back.
    MarkPreChanged();
    MarkChanged();

    return true;
}

bool
UHoudiniAssetInput::OnShouldFilterActor( const AActor * const Actor ) const
{
    if ( !Actor )
        return false;

    if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
    {
        // Only return HoudiniAssetActors
        if ( Actor->IsA<AHoudiniAssetActor>() )
        {
            // But not our own Asset Actor
            if( const USceneComponent* RootComp = Cast<const USceneComponent>( GetHoudiniAssetComponent() ))
            {
                if( RootComp && Cast<AHoudiniAssetActor>( RootComp->GetOwner() ) != Actor )
                    return true;
            }
            return false;
        }
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
    {
        // Only returns Landscape
        if ( Actor->IsA<ALandscapeProxy>() )
        {
            ALandscapeProxy* LandscapeProxy = const_cast<ALandscapeProxy *>(Cast<const ALandscapeProxy>(Actor));
            // But not a landscape generated by this asset
            const UHoudiniAssetComponent * HoudiniAssetComponent = GetHoudiniAssetComponent();
            if (HoudiniAssetComponent && LandscapeProxy && !HoudiniAssetComponent->HasLandscapeActor( LandscapeProxy->GetLandscapeActor() ) )
                return true;
        }
        return false;
    }        
    else if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        for ( auto & OutlinerMesh : InputOutlinerMeshArray )
            if ( OutlinerMesh.ActorPtr.Get() == Actor )
                return true;
    }

    return false;
}

void
UHoudiniAssetInput::OnActorSelected( AActor * Actor )
{
    if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
        return OnInputActorSelected( Actor );
    else if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
        return OnWorldOutlinerActorSelected( Actor );
    else if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
        return OnLandscapeActorSelected( Actor );
}

void
UHoudiniAssetInput::OnInputActorSelected( AActor * Actor )
{
    if ( !Actor && InputAssetComponent )
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Asset Change" ),
            PrimaryObject );
        Modify();

        // Tell the old input asset we are no longer connected.
        InputAssetComponent->RemoveDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );

        // We cleared the selection so just reset all the values.
        InputAssetComponent = nullptr;
        ConnectedAssetId = -1;
    }
    else
    {
        AHoudiniAssetActor * HoudiniAssetActor = (AHoudiniAssetActor *) Actor;
	if (HoudiniAssetActor == nullptr)
	    return;

        UHoudiniAssetComponent * ConnectedHoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();

        // If we just selected the already selected Actor do nothing.
        if ( ConnectedHoudiniAssetComponent == InputAssetComponent )
            return;

        // Do not allow the input asset to be ourself!
        if ( ConnectedHoudiniAssetComponent == PrimaryObject )
            return;

        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Asset Change" ),
            PrimaryObject );
        Modify();

        // Tell the old input asset we are no longer connected.
        if ( InputAssetComponent)
            InputAssetComponent->RemoveDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );

        InputAssetComponent = ConnectedHoudiniAssetComponent;
        ConnectedAssetId = InputAssetComponent->GetAssetId();

        // Mark as disconnected since we need to reconnect to the new asset.
        bInputAssetConnectedInHoudini = false;
    }

    MarkPreChanged();
    MarkChanged();
}

void
UHoudiniAssetInput::OnLandscapeActorSelected( AActor * Actor )
{
    ALandscapeProxy * LandscapeProxy = Cast< ALandscapeProxy >( Actor );
    if ( LandscapeProxy )
    {
        // If we just selected the already selected landscape, do nothing.
        if ( LandscapeProxy == InputLandscapeProxy )
            return;

        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Landscape Change." ),
            PrimaryObject );
        Modify();

        // Store new landscape.
        InputLandscapeProxy = LandscapeProxy;
    }
    else
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Landscape Change." ),
            PrimaryObject );
        Modify();

        InputLandscapeProxy = nullptr;
    }

    MarkPreChanged();
    MarkChanged();
}

void
UHoudiniAssetInput::OnWorldOutlinerActorSelected( AActor * )
{
    // Do nothing.
}

void
UHoudiniAssetInput::TickWorldOutlinerInputs()
{
    // PostLoad initialization must be done on the first tick
    // as some components might now have been fully initialized at PostLoad()
    if ( OutlinerInputsNeedPostLoadInit )
    {
        UpdateInputOulinerArray();

        // The spline Transform array might need to be rebuilt after loading
        for (auto & OutlinerInput : InputOutlinerMeshArray)
        {
            OutlinerInput.RebuildSplineTransformsArrayIfNeeded();
            OutlinerInput.KeepWorldTransform = bKeepWorldTransform;
            OutlinerInput.SplineResolution = UnrealSplineResolution;
        }

        OutlinerInputsNeedPostLoadInit = false;
        return;
    }

    // Lambda use to Modify / Prechange only once
    bool bLocalChanged = false;
    auto MarkLocalChanged = [&]()
    {
        if ( !bLocalChanged )
        {
            Modify();
            MarkPreChanged();
            bLocalChanged = true;
        }
    };
    
    // Refresh the input's component from the actor
    // If the Actor is a blueprint, its component are recreated for every modification
    if ( UpdateInputOulinerArray() )
        MarkLocalChanged();

    //
    if ( bStaticMeshChanged )
        return;

    // Check for destroyed / modified outliner inputs
    for ( auto & OutlinerInput : InputOutlinerMeshArray )
    {
        if ( !OutlinerInput.ActorPtr.IsValid() )
            continue;

        if ( OutlinerInput.HasActorTransformChanged() && ( OutlinerInput.AssetId >= 0 ) )
        {
            MarkLocalChanged();

            // Updates to the new Transform
            UpdateWorldOutlinerTransforms( OutlinerInput );

            HAPI_TransformEuler HapiTransform;
            FMemory::Memzero< HAPI_TransformEuler >( HapiTransform );
            FHoudiniEngineUtils::TranslateUnrealTransform( OutlinerInput.ComponentTransform, HapiTransform );

            HAPI_NodeInfo LocalAssetNodeInfo;
            const HAPI_Result LocalResult = FHoudiniApi::GetNodeInfo(
                FHoudiniEngine::Get().GetSession(), OutlinerInput.AssetId,
                &LocalAssetNodeInfo);

            if ( LocalResult == HAPI_RESULT_SUCCESS )
                FHoudiniApi::SetObjectTransform(
                    FHoudiniEngine::Get().GetSession(),
                    LocalAssetNodeInfo.parentId, &HapiTransform );
        }
        else if ( OutlinerInput.HasComponentTransformChanged() 
                || ( OutlinerInput.HasSplineComponentChanged( UnrealSplineResolution ) )
                || ( OutlinerInput.KeepWorldTransform != bKeepWorldTransform ) )
        {
            MarkLocalChanged();

            // Update to the new Transforms
            UpdateWorldOutlinerTransforms( OutlinerInput );

            // The component or spline has been modified so so we need to indicate that the "static mesh" 
            // has changed in order to rebuild the asset properly in UploadParameterValue()
            bStaticMeshChanged = true;
        }
    }

    if ( bLocalChanged )
        MarkChanged();
}

#endif

void
UHoudiniAssetInput::ConnectInputAssetActor()
{
    if ( InputAssetComponent && FHoudiniEngineUtils::IsValidAssetId( InputAssetComponent->GetAssetId() )
        && !bInputAssetConnectedInHoudini )
    {

        ConnectInputNode();
        InputAssetComponent->AddDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );
        bInputAssetConnectedInHoudini = true;
    }
}

void
UHoudiniAssetInput::DisconnectInputAssetActor()
{
    if ( bInputAssetConnectedInHoudini && !InputAssetComponent )
    {
        if( bIsObjectPathParameter )
        {
            std::string ParamNameString = TCHAR_TO_UTF8( *GetParameterName() );

            FHoudiniApi::SetParmStringValue(
                FHoudiniEngine::Get().GetSession(), NodeId, "",
                ParmId, 0);
        }
        else
        {
            FHoudiniEngineUtils::HapiDisconnectAsset( GetAssetId(), InputIndex );
        }
        bInputAssetConnectedInHoudini = false;
    }
}

void
UHoudiniAssetInput::ConnectLandscapeActor()
{}

void
UHoudiniAssetInput::DisconnectLandscapeActor()
{}

HAPI_NodeId
UHoudiniAssetInput::GetConnectedAssetId() const
{
    return ConnectedAssetId;
}

bool
UHoudiniAssetInput::IsGeometryAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) && ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput ) )
    {
        for ( auto InputObject : InputObjects )
        {
            if ( InputObject )
                return true;
        }
    }

    return false;
}

bool
UHoudiniAssetInput::IsInputAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) && InputAssetComponent && bInputAssetConnectedInHoudini )
    {
        if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
            return true;
    }

    return false;
}

bool
UHoudiniAssetInput::IsCurveAssetConnected() const
{
    if (InputCurve && ( ChoiceIndex == EHoudiniAssetInputType::CurveInput ) )
        return true;

    return false;
}

bool
UHoudiniAssetInput::IsLandscapeAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) )
    {
        if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
            return true;
    }

    return false;
}

bool
UHoudiniAssetInput::IsWorldInputAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) )
    {
        if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
            return true;
    }

    return false;
}

void
UHoudiniAssetInput::OnInputCurveChanged()
{
    MarkPreChanged();
    MarkChanged();
}

void
UHoudiniAssetInput::ExternalDisconnectInputAssetActor()
{
    InputAssetComponent = nullptr;
    ConnectedAssetId = -1;

    MarkPreChanged();
    MarkChanged();
}

bool
UHoudiniAssetInput::DoesInputAssetNeedInstantiation()
{
    if ( ChoiceIndex != EHoudiniAssetInputType::AssetInput )
        return false;

    if ( InputAssetComponent == nullptr )
        return false;

    if ( !FHoudiniEngineUtils::IsValidAssetId(InputAssetComponent->GetAssetId() ) )
        return true;

    return false;
}

UHoudiniAssetComponent *
UHoudiniAssetInput::GetConnectedInputAssetComponent()
{
    return InputAssetComponent;
}

void
UHoudiniAssetInput::NotifyChildParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter )
{
    if ( HoudiniAssetParameter && ChoiceIndex == EHoudiniAssetInputType::CurveInput )
    {
        MarkPreChanged();
        
        if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) )
        {
            // We need to upload changed param back to HAPI.
            if (! HoudiniAssetParameter->UploadParameterValue() )
            {
                HOUDINI_LOG_ERROR( TEXT("%s UploadParameterValue failed"), InputAssetComponent ? *InputAssetComponent->GetOwner()->GetName() : TEXT("unknown") );
            }
        }

        MarkChanged();
    }
}

bool
UHoudiniAssetInput::UpdateInputCurve()
{
    bool Success = true;
    FString CurvePointsString;
    EHoudiniSplineComponentType::Enum CurveTypeValue = EHoudiniSplineComponentType::Bezier;
    EHoudiniSplineComponentMethod::Enum CurveMethodValue = EHoudiniSplineComponentMethod::CVs;
    int32 CurveClosed = 1;

    if(ConnectedAssetId != -1)
    {
        FHoudiniEngineUtils::HapiGetParameterDataAsString(
            ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT( "" ),
            CurvePointsString );
        FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
            ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_TYPE,
            (int32) EHoudiniSplineComponentType::Bezier, (int32 &) CurveTypeValue );
        FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
            ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_METHOD,
            (int32) EHoudiniSplineComponentMethod::CVs, (int32 &) CurveMethodValue );
        FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
            ConnectedAssetId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed );
    }

    // We need to get the NodeInfo to get the parent id
    HAPI_NodeInfo NodeInfo;
    HOUDINI_CHECK_ERROR_RETURN(
        FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &NodeInfo),
        false);

    // Construct geo part object.
    FHoudiniGeoPartObject HoudiniGeoPartObject( ConnectedAssetId, NodeInfo.parentId, ConnectedAssetId, 0 );
    HoudiniGeoPartObject.bIsCurve = true;

    HAPI_AttributeInfo AttributeRefinedCurvePositions;
    TArray< float > RefinedCurvePositions;
    FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION,
        AttributeRefinedCurvePositions, RefinedCurvePositions );

    // Process coords string and extract positions.
    TArray< FVector > CurvePoints;
    FHoudiniEngineUtils::ExtractStringPositions( CurvePointsString, CurvePoints );

    TArray< FVector > CurveDisplayPoints;
    FHoudiniEngineUtils::ConvertScaleAndFlipVectorData( RefinedCurvePositions, CurveDisplayPoints );

    if (InputCurve != nullptr)
    {
        InputCurve->Construct(
            HoudiniGeoPartObject, CurveDisplayPoints, CurveTypeValue, CurveMethodValue,
            (CurveClosed == 1));
    }

    // We also need to construct curve parameters we care about.
    TMap< FString, UHoudiniAssetParameter * > NewInputCurveParameters;

    TArray< HAPI_ParmInfo > ParmInfos;
    ParmInfos.SetNumUninitialized( NodeInfo.parmCount );
    HOUDINI_CHECK_ERROR_RETURN(
        FHoudiniApi::GetParameters(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &ParmInfos[ 0 ], 0, NodeInfo.parmCount ),
        false);

    // Retrieve integer values for this asset.
    TArray< int32 > ParmValueInts;
    ParmValueInts.SetNumZeroed( NodeInfo.parmIntValueCount );
    if ( NodeInfo.parmIntValueCount > 0 )
    {
        HOUDINI_CHECK_ERROR_RETURN(
            FHoudiniApi::GetParmIntValues(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &ParmValueInts[ 0 ], 0, NodeInfo.parmIntValueCount ),
        false );
    }

    // Retrieve float values for this asset.
    TArray< float > ParmValueFloats;
    ParmValueFloats.SetNumZeroed( NodeInfo.parmFloatValueCount );
    if ( NodeInfo.parmFloatValueCount > 0 )
    {
        HOUDINI_CHECK_ERROR_RETURN(
            FHoudiniApi::GetParmFloatValues(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &ParmValueFloats[ 0 ], 0, NodeInfo.parmFloatValueCount ),
        false );
    }

    // Retrieve string values for this asset.
    TArray< HAPI_StringHandle > ParmValueStrings;
    ParmValueStrings.SetNumZeroed( NodeInfo.parmStringValueCount );
    if ( NodeInfo.parmStringValueCount > 0 )
    {
        HOUDINI_CHECK_ERROR_RETURN(
            FHoudiniApi::GetParmStringValues(
                FHoudiniEngine::Get().GetSession(), ConnectedAssetId, true, &ParmValueStrings[ 0 ], 0, NodeInfo.parmStringValueCount ),
        false );
    }

    // Create properties for parameters.
    for ( int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx )
    {
        // Retrieve param info at this index.
        const HAPI_ParmInfo & ParmInfo = ParmInfos[ ParamIdx ];

        // If parameter is invisible, skip it.
        if ( ParmInfo.invisible )
            continue;

        FString LocalParameterName;
        FHoudiniEngineString HoudiniEngineString( ParmInfo.nameSH );
        if ( !HoudiniEngineString.ToFString( LocalParameterName ) )
        {
            // We had trouble retrieving name of this parameter, skip it.
            continue;
        }

        // See if it's one of parameters we are interested in.
        if ( !LocalParameterName.Equals( TEXT( HAPI_UNREAL_PARAM_CURVE_METHOD ) ) &&
            !LocalParameterName.Equals( TEXT( HAPI_UNREAL_PARAM_CURVE_TYPE ) ) &&
            !LocalParameterName.Equals( TEXT( HAPI_UNREAL_PARAM_CURVE_CLOSED ) ) )
        {
            // Not parameter we are interested in.
            continue;
        }

        // See if this parameter has already been created.
        UHoudiniAssetParameter * const * FoundHoudiniAssetParameter = InputCurveParameters.Find( LocalParameterName );
        UHoudiniAssetParameter * HoudiniAssetParameter = nullptr;

        // If parameter exists, we can reuse it.
        if ( FoundHoudiniAssetParameter )
        {
            HoudiniAssetParameter = *FoundHoudiniAssetParameter;

            // Remove parameter from current map.
            InputCurveParameters.Remove( LocalParameterName );

            // Reinitialize parameter and add it to map.
            HoudiniAssetParameter->CreateParameter( nullptr, this, ConnectedAssetId, ParmInfo );
            NewInputCurveParameters.Add( LocalParameterName, HoudiniAssetParameter );
            continue;
        }
        else
        {
            if ( ParmInfo.type == HAPI_PARMTYPE_INT )
            {
                if ( !ParmInfo.choiceCount )
                    HoudiniAssetParameter = UHoudiniAssetParameterInt::Create( nullptr, this, ConnectedAssetId, ParmInfo );
                else
                    HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create( nullptr, this, ConnectedAssetId, ParmInfo );
            }
            else if ( ParmInfo.type == HAPI_PARMTYPE_TOGGLE )
            {
                HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create( nullptr, this, ConnectedAssetId, ParmInfo );
            }
            else
            {
                Success = false;
                check( false );
            }

            NewInputCurveParameters.Add( LocalParameterName, HoudiniAssetParameter );
        }
    }

    ClearInputCurveParameters();
    InputCurveParameters = NewInputCurveParameters;

    if ( bSwitchedToCurve )
    {

#if WITH_EDITOR
        // We need to trigger details panel update.
        OnParamStateChanged();

        // The editor caches the current selection visualizer, so we need to trick
        // and pretend the selection has changed so that the HSplineVisualizer can be drawn immediately
        if (GUnrealEd)
            GUnrealEd->NoteSelectionChange();
#endif

        bSwitchedToCurve = false;
    }
    return Success;
}

FText
UHoudiniAssetInput::HandleChoiceContentText() const
{
    return FText::FromString( ChoiceStringValue );
}

#if WITH_EDITOR

void
UHoudiniAssetInput::CheckStateChangedExportOnlySelected( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportSelectionOnly != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Selected Landscape Components mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportSelectionOnly = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportOnlySelected() const
{
    if ( bLandscapeExportSelectionOnly )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedAutoSelectLandscape( ECheckBoxState NewState )
{
    int32 bState = (NewState == ECheckBoxState::Checked);

    if (bLandscapeAutoSelectComponent != bState)
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT(HOUDINI_MODULE_RUNTIME),
            LOCTEXT("HoudiniInputChange", "Houdini Export Auto-Select Landscape Components mode change."),
            PrimaryObject);
        Modify();

        MarkPreChanged();

        bLandscapeAutoSelectComponent = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedAutoSelectLandscape() const
{
    if ( bLandscapeAutoSelectComponent )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportCurves( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportCurves != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Landscape Curve mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportCurves = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportCurves() const
{
    if ( bLandscapeExportCurves )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportAsMesh( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportAsMesh != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Landscape As Mesh mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportAsMesh = bState;

        if ( bState )
            bLandscapeExportAsHeightfield = false;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportAsMesh() const
{
    if ( bLandscapeExportAsMesh )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportAsHeightfield( ECheckBoxState NewState ) 
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportAsHeightfield != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT(HOUDINI_MODULE_RUNTIME),
            LOCTEXT("HoudiniInputChange", "Houdini Export Landscape As Heightfield mode change."),
            PrimaryObject);
        Modify();

        MarkPreChanged();

        bLandscapeExportAsHeightfield = bState;
        if ( bState )
            bLandscapeExportAsMesh = false;
        else
            bLandscapeExportAsMesh = true;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportAsHeightfield() const
{
    if ( bLandscapeExportAsHeightfield )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportAsPoints( ECheckBoxState NewState )
{
    int32 bState = (NewState == ECheckBoxState::Checked);

    uint32 bExportAsPoints = !bLandscapeExportAsHeightfield && !bLandscapeExportAsMesh;
    if (bExportAsPoints != bState)
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT(HOUDINI_MODULE_RUNTIME),
            LOCTEXT("HoudiniInputChange", "Houdini Export Landscape As Points mode change."),
            PrimaryObject);
        Modify();

        MarkPreChanged();

        if ( bState )
        {
            bLandscapeExportAsHeightfield = false;
            bLandscapeExportAsMesh = false;
        }
        else
        {
            bLandscapeExportAsHeightfield = true;
            bLandscapeExportAsMesh = false;
        }

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportAsPoints() const
{
    if ( !bLandscapeExportAsHeightfield && !bLandscapeExportAsMesh )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportMaterials( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportMaterials != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Landscape Materials mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportMaterials = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportMaterials() const
{
    if ( bLandscapeExportMaterials )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportLighting( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportLighting != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Landscape Lighting mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportLighting = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportLighting() const
{
    if ( bLandscapeExportLighting )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportNormalizedUVs( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportNormalizedUVs != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Landscape Normalized UVs mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportNormalizedUVs = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportNormalizedUVs() const
{
    if ( bLandscapeExportNormalizedUVs )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportTileUVs( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bLandscapeExportTileUVs != bState )
    {
        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Export Landscape Tile UVs mode change." ),
            PrimaryObject );
        Modify();

        MarkPreChanged();

        bLandscapeExportTileUVs = bState;

        // Mark this parameter as changed.
        MarkChanged();
    }
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedExportTileUVs() const
{
    if ( bLandscapeExportTileUVs )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedKeepWorldTransform(ECheckBoxState NewState)
{ 
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bKeepWorldTransform == bState )
        return;

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_RUNTIME),
        LOCTEXT("HoudiniInputChange", "Houdini Input Transform Type change."),
        PrimaryObject);
    Modify();

    MarkPreChanged();

    bKeepWorldTransform = bState;

    // Mark this parameter as changed.
    MarkChanged();
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedKeepWorldTransform() const
{
    if ( bKeepWorldTransform == 2 )
    {
        if (GetDefaultTranformTypeValue())
            return ECheckBoxState::Checked;
        else
            return ECheckBoxState::Unchecked;
    }
    else if ( bKeepWorldTransform )
        return ECheckBoxState::Checked;
    
    return ECheckBoxState::Unchecked;
}


void
UHoudiniAssetInput::CheckStateChangedPackBeforeMerge( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bPackBeforeMerge == bState )
        return;

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Pack Before Merge changed." ),
        PrimaryObject );
    Modify();

    MarkPreChanged();

    bPackBeforeMerge = bState;

    // Mark this parameter as changed.
    MarkChanged( true );
}


ECheckBoxState
UHoudiniAssetInput::IsCheckedPackBeforeMerge() const
{
    if ( bPackBeforeMerge )
        return ECheckBoxState::Checked;
    else
        return ECheckBoxState::Unchecked;
}

FReply
UHoudiniAssetInput::OnButtonClickRecommit()
{
    // There's no undo operation for button.
    MarkPreChanged();
    MarkChanged();

    return FReply::Handled();
}

FReply
UHoudiniAssetInput::OnButtonClickSelectActors()
{
    // There's no undo operation for button.

    FPropertyEditorModule & PropertyModule =
        FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

    // Locate the details panel.
    FName DetailsPanelName = "LevelEditorSelectionDetails";
    TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

    if ( !DetailsView.IsValid() )
        return FReply::Handled();

    class SLocalDetailsView : public SDetailsViewBase
    {
        public:
        void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
        void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
    };
    auto * LocalDetailsView = static_cast< SLocalDetailsView * >( DetailsView.Get() );

    if ( !DetailsView->IsLocked() )
    {
        LocalDetailsView->LockDetailsView();
        check( DetailsView->IsLocked() );

        // Force refresh of details view.
        OnParamStateChanged();

        // Select the previously chosen input Actors from the World Outliner.
        GEditor->SelectNone( false, true );
        for ( auto & OutlinerMesh : InputOutlinerMeshArray )
        {
            if ( OutlinerMesh.ActorPtr.IsValid() )
                GEditor->SelectActor( OutlinerMesh.ActorPtr.Get(), true, true );
        }

        return FReply::Handled();
    }

    if ( !GEditor || !GEditor->GetSelectedObjects() )
        return FReply::Handled();

    // If details panel is locked, locate selected actors and check if this component belongs to one of them.

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini World Outliner Input Change" ),
        PrimaryObject );
    Modify();

    MarkPreChanged();
    bStaticMeshChanged = true;

    // Delete all assets and reset the array.
    // TODO: Make this process a little more efficient.
    DisconnectAndDestroyInputAsset();
    InputOutlinerMeshArray.Empty();

    USelection * SelectedActors = GEditor->GetSelectedActors();

    // If the builder brush is selected, first deselect it.
    for ( FSelectionIterator It( *SelectedActors ); It; ++It )
    {
        AActor * Actor = Cast< AActor >( *It );
        if ( !Actor )
            continue;

        // Don't allow selection of ourselves. Bad things happen if we do.
        if ( GetHoudiniAssetComponent() && ( Actor == GetHoudiniAssetComponent()->GetOwner() ) )
            continue;

        UpdateInputOulinerArrayFromActor( Actor, false );
    }

    MarkChanged();

    AActor* HoudiniAssetActor = GetHoudiniAssetComponent()->GetOwner();

    if ( DetailsView->IsLocked() )
    {
        LocalDetailsView->UnlockDetailsView();
        check( !DetailsView->IsLocked() );

        TArray< UObject * > DummySelectedActors;
        DummySelectedActors.Add( HoudiniAssetActor );

        // Reset selected actor to itself, force refresh and override the lock.
        DetailsView->SetObjects( DummySelectedActors, true, true );
    }

    // Reselect the Asset Actor. If we don't do this, our Asset parameters will stop
    // refreshing and the user will be very confused. It is also resetting the state
    // of the selection before the input actor selection process was started.
    GEditor->SelectNone( false, true );
    GEditor->SelectActor( HoudiniAssetActor, true, true );

    // Update parameter layout.
    OnParamStateChanged();

    // Start or stop the tick timer to check if the selected Actors have been transformed.
    if ( InputOutlinerMeshArray.Num() > 0 )
        StartWorldOutlinerTicking();
    else if ( InputOutlinerMeshArray.Num() <= 0 )
        StopWorldOutlinerTicking();

    return FReply::Handled();
}

void
UHoudiniAssetInput::StartWorldOutlinerTicking()
{
    if ( InputOutlinerMeshArray.Num() > 0 && !WorldOutlinerTimerDelegate.IsBound() && GEditor )
    {
        WorldOutlinerTimerDelegate = FTimerDelegate::CreateUObject( this, &UHoudiniAssetInput::TickWorldOutlinerInputs );

        // We need to register delegate with the timer system.
        static const float TickTimerDelay = 0.5f;
        GEditor->GetTimerManager()->SetTimer( WorldOutlinerTimerHandle, WorldOutlinerTimerDelegate, TickTimerDelay, true );
    }
}

void
UHoudiniAssetInput::StopWorldOutlinerTicking()
{
    if ( InputOutlinerMeshArray.Num() <= 0 && WorldOutlinerTimerDelegate.IsBound() && GEditor )
    {
        GEditor->GetTimerManager()->ClearTimer( WorldOutlinerTimerHandle );
        WorldOutlinerTimerDelegate.Unbind();
    }
}

void UHoudiniAssetInput::InvalidateNodeIds()
{
    ConnectedAssetId = -1;
    for (auto& OutlinerInputMesh : InputOutlinerMeshArray)
    {
        OutlinerInputMesh.AssetId = -1;
    }
}

void UHoudiniAssetInput::DuplicateCurves(UHoudiniAssetInput * OriginalInput)
{
    if (!InputCurve || !OriginalInput)
        return;

    USceneComponent* RootComp = GetHoudiniAssetComponent();
    if( !RootComp )
        return;

    // The previous call to DuplicateObject did not duplicate the curves properly
    // Both the original and duplicated Inputs now share the same InputCurve, so we 
    // need to create a proper copy of that curve

    // Keep the original pointer to the curve, as we need to duplicate its data
    UHoudiniSplineComponent* pOriginalCurve = InputCurve;

    // Creates a new Curve
    InputCurve = NewObject< UHoudiniSplineComponent >(
        RootComp->GetOwner(), UHoudiniSplineComponent::StaticClass(),
        NAME_None, RF_Public | RF_Transactional);

    // Attach curve component to asset.
    InputCurve->AttachToComponent( RootComp, FAttachmentTransformRules::KeepRelativeTransform);
    InputCurve->RegisterComponent();
    InputCurve->SetVisibility(true);

    // The new curve need do know that it is connected to this Input
    InputCurve->SetHoudiniAssetInput(this);
    
    // The call to DuplicateObject has actually modified the original object's Input
    // so we need to fix that as well.
    pOriginalCurve->SetHoudiniAssetInput(OriginalInput);

    // "Copy" the old curves parameters to the new one
    InputCurve->CopyFrom(pOriginalCurve);

    // to force rebuild...
    bSwitchedToCurve = true;
}

void 
UHoudiniAssetInput::RemoveWorldOutlinerInput( int32 AtIndex )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini World Outliner Input Change" ),
        PrimaryObject );
    Modify();

    MarkPreChanged();
    bStaticMeshChanged = true;
    InputOutlinerMeshArray.RemoveAt( AtIndex );
    MarkChanged();
}

void
UHoudiniAssetInput::UpdateWorldOutlinerTransforms( FHoudiniAssetInputOutlinerMesh& OutlinerMesh )
{
    // Update to the new Transforms
    OutlinerMesh.ActorTransform = OutlinerMesh.ActorPtr->GetTransform();

    if (OutlinerMesh.StaticMeshComponent)
        OutlinerMesh.ComponentTransform = OutlinerMesh.StaticMeshComponent->GetComponentTransform();

    if (OutlinerMesh.SplineComponent)
        OutlinerMesh.ComponentTransform = OutlinerMesh.SplineComponent->GetComponentTransform();

    OutlinerMesh.KeepWorldTransform = bKeepWorldTransform;
}

void UHoudiniAssetInput::OnAddToInputObjects()
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
        PrimaryObject );
    Modify();
    MarkPreChanged();
    InputObjects.Add( nullptr );
    InputTransforms.Add( FTransform::Identity );
    TransformUIExpanded.Add( false );
    MarkChanged();
    bStaticMeshChanged = true;
    OnParamStateChanged();
}

void UHoudiniAssetInput::OnEmptyInputObjects()
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
        PrimaryObject );
    Modify();
    MarkPreChanged();

    // Empty the arrays
    InputObjects.Empty();
    InputTransforms.Empty();
    TransformUIExpanded.Empty();

    // To avoid displaying 0 elements when there's one (empty), initialize the array
    InputObjects.Add( nullptr );
    InputTransforms.Add( FTransform::Identity );
    TransformUIExpanded.Add( false );
    MarkChanged();
    bStaticMeshChanged = true;
    OnParamStateChanged();
}


TOptional< float >
UHoudiniAssetInput::GetSplineResolutionValue() const
{
    if (UnrealSplineResolution != -1.0f)
        return UnrealSplineResolution;

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (HoudiniRuntimeSettings)
        return HoudiniRuntimeSettings->MarshallingSplineResolution;
    else
        return HAPI_UNREAL_PARAM_SPLINE_RESOLUTION_DEFAULT;
}


void
UHoudiniAssetInput::SetSplineResolutionValue(float InValue)
{
    if (InValue < 0)
        OnResetSplineResolutionClicked();
    else
        UnrealSplineResolution = FMath::Clamp< float >(InValue, 0.0f, 10000.0f);
}


bool
UHoudiniAssetInput::IsSplineResolutionEnabled() const
{
    if (ChoiceIndex != EHoudiniAssetInputType::WorldInput)
        return false;

    for (int32 n = 0; n < InputOutlinerMeshArray.Num(); n++)
    {
        if (InputOutlinerMeshArray[n].SplineComponent)
            return true;
    }

    return false;
}


FReply
UHoudiniAssetInput::OnResetSplineResolutionClicked()
{
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if (HoudiniRuntimeSettings)
        UnrealSplineResolution = HoudiniRuntimeSettings->MarshallingSplineResolution;
    else
        UnrealSplineResolution = HAPI_UNREAL_PARAM_SPLINE_RESOLUTION_DEFAULT;

    return FReply::Handled();
}

FMenuBuilder
UHoudiniAssetInput::CreateCustomActorPickerWidget( const TAttribute<FText>& HeadingText, const bool& bShowCurrentSelectionSection )
{
    // Custom Actor Picker showing only the desired Actor types.
    // Note: Code stolen from SPropertyMenuActorPicker.cpp
    FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateUObject( this, &UHoudiniAssetInput::OnShouldFilterActor );
        
    //FHoudiniEngineEditor & HoudiniEngineEditor = FHoudiniEngineEditor::Get();
    //TSharedPtr< ISlateStyle > StyleSet = GEditor->GetSlateStyle();
        
    FMenuBuilder MenuBuilder( true, NULL );

    if ( bShowCurrentSelectionSection )
    {
        MenuBuilder.BeginSection( NAME_None, LOCTEXT( "CurrentActorOperationsHeader", "Current Selection" ) );
        {
            MenuBuilder.AddMenuEntry(
                TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(this, &UHoudiniAssetInput::GetCurrentSelectionText)),
                TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(this, &UHoudiniAssetInput::GetCurrentSelectionText)),
                FSlateIcon(),//StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo")),
                FUIAction(),
                NAME_None,
                EUserInterfaceActionType::Button,
                NAME_None);
        }
        MenuBuilder.EndSection();
    }

    MenuBuilder.BeginSection(NAME_None, HeadingText );
    {
        TSharedPtr< SWidget > MenuContent;

        FSceneOutlinerModule & SceneOutlinerModule =
            FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >( TEXT( "SceneOutliner" ) );

        SceneOutliner::FInitializationOptions InitOptions;
        InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
        InitOptions.Filters->AddFilterPredicate( ActorFilter );
        InitOptions.bFocusSearchBoxWhenOpened = true;
        
        static const FVector2D SceneOutlinerWindowSize( 350.0f, 200.0f );
        MenuContent =
            SNew( SBox )
            .WidthOverride( SceneOutlinerWindowSize.X )
            .HeightOverride( SceneOutlinerWindowSize.Y )
            [
                SNew( SBorder )
                .BorderImage( FEditorStyle::GetBrush( "Menu.Background" ) )
                [
                    SceneOutlinerModule.CreateSceneOutliner(
                        InitOptions, FOnActorPicked::CreateUObject(
                            this, &UHoudiniAssetInput::OnActorSelected ) )
                ]
            ];

        MenuBuilder.AddWidget( MenuContent.ToSharedRef(), FText::GetEmpty(), true );
    }
    MenuBuilder.EndSection();

    return MenuBuilder;
}

FText
UHoudiniAssetInput::GetCurrentSelectionText() const
{
    FText CurrentSelectionText;
    if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
    {
        if ( InputAssetComponent && InputAssetComponent->GetHoudiniAssetActorOwner() )
        {
            CurrentSelectionText = FText::FromString( InputAssetComponent->GetHoudiniAssetActorOwner()->GetName() );
        }
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
    {
        if ( InputLandscapeProxy )
        {
            CurrentSelectionText = FText::FromString( InputLandscapeProxy->GetName() );
        }
    }

    return CurrentSelectionText;
}

#endif

FArchive &
operator<<( FArchive & Ar, FHoudiniAssetInputOutlinerMesh & HoudiniAssetInputOutlinerMesh )
{
    HoudiniAssetInputOutlinerMesh.Serialize( Ar );
    return Ar;
}

FBox
UHoudiniAssetInput::GetInputBounds()
{
    FBox Bounds( ForceInitToZero );

    if ( IsCurveAssetConnected() && InputCurve )
    {
        TArray<FVector> CurvePositions;
        InputCurve->GetCurvePositions( CurvePositions );

        for ( int32 n = 0; n < CurvePositions.Num(); n++ )
            Bounds += CurvePositions[ n ];
    }

    if ( IsWorldInputAssetConnected() )
    {
        for (int32 n = 0; n < InputOutlinerMeshArray.Num(); n++)
        {
            if ( !InputOutlinerMeshArray[ n ].ActorPtr.IsValid() )
                continue;

            FVector Origin, Extent;
            InputOutlinerMeshArray[ n ].ActorPtr->GetActorBounds( false, Origin, Extent );

            Bounds += FBox::BuildAABB( Origin, Extent );
        }
    }

    if ( IsInputAssetConnected() && InputAssetComponent )
    {
        Bounds += InputAssetComponent->GetAssetBounds();
    }

    if ( IsLandscapeAssetConnected() && InputLandscapeProxy )
    {
        FVector Origin, Extent;
        InputLandscapeProxy->GetActorBounds( false, Origin, Extent );

        Bounds += FBox::BuildAABB( Origin, Extent );
    }

    return Bounds;
}

void UHoudiniAssetInput::SetDefaultInputTypeFromLabel()
{
#if WITH_EDITOR
    // Look if we can find an input type prefix in the input name
    FString inputName = GetParameterLabel();

    // We'll try to find these magic words to try to detect the default input type
    //FString geoPrefix = TEXT("geo");
    FString curvePrefix		= TEXT( "curve" );
    FString landscapePrefix	= TEXT( "landscape" );
    FString landscapePrefix2	= TEXT( "terrain" );
    FString landscapePrefix3	= TEXT( "heightfield" );
    FString worldPrefix		= TEXT( "world" );
    FString worldPrefix2	= TEXT( "outliner" );
    FString assetPrefix		= TEXT( "asset" );
    FString assetPrefix2	= TEXT( "hda" );

    if ( inputName.Contains( curvePrefix, ESearchCase::IgnoreCase ) )
        ChangeInputType( EHoudiniAssetInputType::CurveInput );

    else if ( ( inputName.Contains( landscapePrefix, ESearchCase::IgnoreCase ) ) 
            || ( inputName.Contains( landscapePrefix2, ESearchCase::IgnoreCase ) )
            || ( inputName.Contains( landscapePrefix3, ESearchCase::IgnoreCase ) ) )
        ChangeInputType(EHoudiniAssetInputType::LandscapeInput);

    else if ( ( inputName.Contains( worldPrefix, ESearchCase::IgnoreCase ) )
            || ( inputName.Contains( worldPrefix2, ESearchCase::IgnoreCase ) ) )
        ChangeInputType(EHoudiniAssetInputType::WorldInput);

    else if ( ( inputName.Contains( assetPrefix, ESearchCase::IgnoreCase ) )
            || ( inputName.Contains( assetPrefix, ESearchCase::IgnoreCase ) ) )
        ChangeInputType(EHoudiniAssetInputType::AssetInput);

    else
    {
        // By default, geometry input is chosen.
        ChoiceIndex = EHoudiniAssetInputType::GeometryInput;
    }
#else
    ChoiceIndex = EHoudiniAssetInputType::GeometryInput;
#endif
}

bool
UHoudiniAssetInput::HasChanged() const
{
    // Inputs should be considered changed after being loaded
    return bChanged || bLoadedParameter || !bInputAssetConnectedInHoudini;
}

#if WITH_EDITOR
bool
UHoudiniAssetInput::UpdateInputOulinerArray()
{
    bool bChanged = false;

    // See if some outliner inputs need to be updated, or removed
    // If an input's Actor is no longer valid, then when need to remove that input.
    // If an input's Components needs to be updated (are no longer valid), then all the components 
    // from the same actor needs to be updated as well.
    // This can happen for example when a blueprint actor is modified/serialized etc..
    TArray<AActor *> ActorToUpdateArray;
    for ( int32 n = InputOutlinerMeshArray.Num() - 1; n >= 0; n-- )
    {
        FHoudiniAssetInputOutlinerMesh OutlinerInput = InputOutlinerMeshArray[ n ];
        if ( !OutlinerInput.ActorPtr.IsValid() )
        {
            // This input has an invalid actor: destroy it and its asset
            bChanged = true;

            // Destroy Houdini asset
            if ( FHoudiniEngineUtils::IsValidAssetId( OutlinerInput.AssetId ) )
            {
                FHoudiniEngineUtils::DestroyHoudiniAsset( OutlinerInput.AssetId );
                OutlinerInput.AssetId = -1;
            }

            // Remove that input
            InputOutlinerMeshArray.RemoveAt( n );
        }
        else
        {
            // This input has a valid actor, see if its component needs to be updated
            if ( !OutlinerInput.NeedsComponentUpdate() )
                continue;

            if ( ActorToUpdateArray.Contains( OutlinerInput.ActorPtr.Get() ) )
                continue;

            ActorToUpdateArray.Add( OutlinerInput.ActorPtr.Get() );

            bChanged = true;
        }
    }

    // Creates the inputs from the actors
    for ( auto & CurrentActor : ActorToUpdateArray )
        UpdateInputOulinerArrayFromActor( CurrentActor, true );

    return bChanged;
}

void
UHoudiniAssetInput::UpdateInputOulinerArrayFromActor( AActor * Actor, const bool& NeedCleanUp )
{
    if ( !Actor )
        return;

    // Don't allow selection of ourselves. Bad things happen if we do.
    if ( GetHoudiniAssetComponent() && ( Actor == GetHoudiniAssetComponent()->GetOwner() ) )
        return;

    // Destroy previous outliner inputs from this actor if needed
    if ( NeedCleanUp )
    {
        for ( int32 n = InputOutlinerMeshArray.Num() - 1; n >= 0; n-- )
        {
            if ( InputOutlinerMeshArray[ n ].ActorPtr.Get() != Actor )
                continue;

            InputOutlinerMeshArray.RemoveAt( n );
        }
    }

    // Looking for StaticMeshes
    for ( UActorComponent * Component : Actor->GetComponentsByClass( UStaticMeshComponent::StaticClass() ) )
    {
        UStaticMeshComponent * StaticMeshComponent = CastChecked< UStaticMeshComponent >( Component );
        if ( !StaticMeshComponent )
            continue;

        UStaticMesh * StaticMesh = StaticMeshComponent->GetStaticMesh();
        if ( !StaticMesh )
            continue;

        // Add the mesh to the array
        FHoudiniAssetInputOutlinerMesh OutlinerMesh;
        OutlinerMesh.ActorPtr = Actor;
        OutlinerMesh.StaticMeshComponent = StaticMeshComponent;
        OutlinerMesh.StaticMesh = StaticMesh;
        OutlinerMesh.SplineComponent = nullptr;
        OutlinerMesh.AssetId = -1;

        UpdateWorldOutlinerTransforms( OutlinerMesh );

        InputOutlinerMeshArray.Add( OutlinerMesh );
    }

    // Looking for Splines
    for ( UActorComponent * Component : Actor->GetComponentsByClass( USplineComponent::StaticClass() ) )
    {
        USplineComponent * SplineComponent = CastChecked< USplineComponent >( Component );
        if ( !SplineComponent )
            continue;

        // Add the spline to the array
        FHoudiniAssetInputOutlinerMesh OutlinerMesh;

        OutlinerMesh.ActorPtr = Actor;
        OutlinerMesh.StaticMeshComponent = nullptr;
        OutlinerMesh.StaticMesh = nullptr;
        OutlinerMesh.SplineComponent = SplineComponent;
        OutlinerMesh.AssetId = -1;

        UpdateWorldOutlinerTransforms( OutlinerMesh );

        // Updating the OutlinerMesh's struct infos
        OutlinerMesh.SplineResolution = UnrealSplineResolution;
        OutlinerMesh.SplineLength = SplineComponent->GetSplineLength();
        OutlinerMesh.NumberOfSplineControlPoints = SplineComponent->GetNumberOfSplinePoints();

        InputOutlinerMeshArray.Add( OutlinerMesh );
    }
}

/** Delegate: Gets the image for the expander button */
const FSlateBrush* UHoudiniAssetInput::GetExpanderImage( int32 AtIndex ) const
{
    FName ResourceName;
    if ( TransformUIExpanded[ AtIndex ] )
    {
        if ( ExpanderArrow->IsHovered() )
        {
            ResourceName = "TreeArrow_Expanded_Hovered";
        }
        else
        {
            ResourceName = "TreeArrow_Expanded";
        }
    }
    else
    {
        if ( ExpanderArrow->IsHovered() )
        {
            ResourceName = "TreeArrow_Collapsed_Hovered";
        }
        else
        {
            ResourceName = "TreeArrow_Collapsed";
        }
    }

    return FEditorStyle::GetBrush( ResourceName );
}

FReply
UHoudiniAssetInput::OnExpandInputTransform( int32 AtIndex )
{
    if ( TransformUIExpanded.IsValidIndex( AtIndex ) )
    {
        TransformUIExpanded[ AtIndex ] = !TransformUIExpanded[ AtIndex ];
        OnParamStateChanged();
    }

    return FReply::Handled();
}

TOptional< float >
UHoudiniAssetInput::GetPositionX( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.GetLocation().X;
}

TOptional< float >
UHoudiniAssetInput::GetPositionY( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.GetLocation().Y;
}

TOptional< float >
UHoudiniAssetInput::GetPositionZ( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.GetLocation().Z;
}

void 
UHoudiniAssetInput::SetPositionX( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FVector Position = InputTransforms[ AtIndex ].GetLocation();
    if ( Position.X == Value )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();

    Position.X = Value;
    InputTransforms[ AtIndex ].SetLocation( Position );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

void 
UHoudiniAssetInput::SetPositionY( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FVector Position = InputTransforms[ AtIndex ].GetLocation();
    if ( Position.Y == Value )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();

    Position.Y = Value;
    InputTransforms[ AtIndex ].SetLocation( Position );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

void 
UHoudiniAssetInput::SetPositionZ( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FVector Position = InputTransforms[ AtIndex ].GetLocation();
    if ( Position.Z == Value )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();

    Position.Z = Value;
    InputTransforms[ AtIndex ].SetLocation( Position );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

TOptional< float >
UHoudiniAssetInput::GetRotationRoll( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.Rotator().Roll;
}

TOptional< float >
UHoudiniAssetInput::GetRotationPitch( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.Rotator().Pitch;
}

TOptional< float >
UHoudiniAssetInput::GetRotationYaw( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.Rotator().Yaw;
}

void
UHoudiniAssetInput::SetRotationRoll( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FRotator Rotator = InputTransforms[ AtIndex ].Rotator();    
    if ( FMath::IsNearlyEqual( Rotator.Roll, Value, SMALL_NUMBER ) )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );
    
    Modify();

    Rotator.Roll = Value;
    InputTransforms[ AtIndex ].SetRotation( Rotator.Quaternion() );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

void
UHoudiniAssetInput::SetRotationPitch( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FRotator Rotator = InputTransforms[ AtIndex ].Rotator();    
    if ( FMath::IsNearlyEqual( Rotator.Pitch, Value, SMALL_NUMBER ) )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this);

    Modify();

    Rotator.Pitch = Value;
    InputTransforms[ AtIndex ].SetRotation( Rotator.Quaternion() );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

void
UHoudiniAssetInput::SetRotationYaw( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FRotator Rotator = InputTransforms[ AtIndex ].Rotator();    
    if ( FMath::IsNearlyEqual( Rotator.Yaw, Value, SMALL_NUMBER ) )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();

    Rotator.Yaw = Value;
    InputTransforms[ AtIndex ].SetRotation( Rotator.Quaternion() );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

/** Returns the input's transform scale values **/
TOptional< float > 
UHoudiniAssetInput::GetScaleX( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.GetScale3D().X;
}

TOptional< float >
UHoudiniAssetInput::GetScaleY( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.GetScale3D().Y;
}

TOptional< float >
UHoudiniAssetInput::GetScaleZ( int32 AtIndex ) const
{
    FTransform transform = FTransform::Identity;
    if ( InputTransforms.IsValidIndex( AtIndex ) )
        transform = InputTransforms[ AtIndex ];

    return transform.GetScale3D().Z;
}

void 
UHoudiniAssetInput::SetScaleX( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FVector Scale = InputTransforms[ AtIndex ].GetScale3D();
    if ( Scale.X == Value )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();

    Scale.X = Value;
    InputTransforms[ AtIndex ].SetScale3D( Scale );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

void
UHoudiniAssetInput::SetScaleY( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FVector Scale = InputTransforms[ AtIndex ].GetScale3D();
    if ( Scale.Y == Value )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();
    
    Scale.Y = Value;
    InputTransforms[ AtIndex ].SetScale3D( Scale );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

void
UHoudiniAssetInput::SetScaleZ( float Value, int32 AtIndex )
{
    if ( !InputTransforms.IsValidIndex( AtIndex ) )
        return;

    FVector Scale = InputTransforms[ AtIndex ].GetScale3D();
    if ( Scale.Z == Value )
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Change" ),
        this );

    Modify();

    Scale.Z = Value;
    InputTransforms[ AtIndex ].SetScale3D( Scale );

    MarkChanged( true );
    bStaticMeshChanged = true;
}

#endif

#undef LOCTEXT_NAMESPACE