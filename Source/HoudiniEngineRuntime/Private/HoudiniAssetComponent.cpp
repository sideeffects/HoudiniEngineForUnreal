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
*/

#include "HoudiniAssetComponent.h"

#include "HoudiniApi.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineMaterialUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniAssetInput.h"
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterButton.h"
#include "HoudiniAssetParameterChoice.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniAssetParameterFolder.h"
#include "HoudiniAssetParameterFolderList.h"
#include "HoudiniAssetParameterInt.h"
#include "HoudiniAssetParameterLabel.h"
#include "HoudiniAssetParameterMultiparm.h"
#include "HoudiniAssetParameterSeparator.h"
#include "HoudiniAssetParameterString.h"
#include "HoudiniAssetParameterFile.h"
#include "HoudiniAssetParameterToggle.h"
#include "HoudiniAssetParameterRamp.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniEngineTask.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniAssetComponentMaterials.h"
#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniEngineString.h"
#include "HoudiniAssetInstanceInputField.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniParamUtils.h"
#include "HoudiniLandscapeUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Landscape.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInstance.h"
#include "Engine/StaticMeshSocket.h"
#include "HoudiniCookHandler.h"
#include "UObject/MetaData.h"
#if WITH_EDITOR
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor.h"
#include "EdMode.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/UnrealEd/Private/GeomFitUtils.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Editor/UnrealEd/Public/BusyCursor.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "Editor/PropertyEditor/Private/PropertyNode.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "StaticMeshResources.h"
#include "Framework/Application/SlateApplication.h"
#include "NavigationSystem.h"
#include "FileHelpers.h"
#endif
#include "Internationalization/Internationalization.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

#if WITH_EDITOR

 /** Slate widget used to pick an asset to instantiate from an HDA with multiple assets inside. **/
class SAssetSelectionWidget : public SCompoundWidget
{
    public:
        SLATE_BEGIN_ARGS( SAssetSelectionWidget )
            : _WidgetWindow(), _AvailableAssetNames()
        {}

        SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
            SLATE_ARGUMENT( TArray< HAPI_StringHandle >, AvailableAssetNames )
            SLATE_END_ARGS()

    public:

        SAssetSelectionWidget();

    public:

        /** Widget construct. **/
        void Construct( const FArguments & InArgs );

        /** Return true if cancel button has been pressed. **/
        bool IsCancelled() const;

        /** Return true if constructed widget is valid. **/
        bool IsValidWidget() const;

        /** Return selected asset name. **/
        int32 GetSelectedAssetName() const;

    protected:

        /** Called when Ok button is pressed. **/
        FReply OnButtonOk();

        /** Called when Cancel button is pressed. **/
        FReply OnButtonCancel();

        /** Called when user picks an asset. **/
        FReply OnButtonAssetPick( int32 AssetName );

    protected:

        /** Parent widget window. **/
        TSharedPtr< SWindow > WidgetWindow;

        /** List of available Houdini Engine asset names. **/
        TArray< HAPI_StringHandle > AvailableAssetNames;

        /** Selected asset name. **/
        int32 SelectedAssetName;

        /** Is set to true if constructed widget is valid. **/
        bool bIsValidWidget;

        /** Is set to true if selection process has been cancelled. **/
        bool bIsCancelled;
};

SAssetSelectionWidget::SAssetSelectionWidget()
    : SelectedAssetName( -1 )
    , bIsValidWidget( false )
    , bIsCancelled( false )
{}

bool
SAssetSelectionWidget::IsCancelled() const
{
    return bIsCancelled;
}

bool
SAssetSelectionWidget::IsValidWidget() const
{
    return bIsValidWidget;
}

int32
SAssetSelectionWidget::GetSelectedAssetName() const
{
    return SelectedAssetName;
}

void
SAssetSelectionWidget::Construct( const FArguments & InArgs )
{
    WidgetWindow = InArgs._WidgetWindow;
    AvailableAssetNames = InArgs._AvailableAssetNames;

    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

    this->ChildSlot
    [
        SNew( SBorder )
        .BorderImage( FEditorStyle::GetBrush( TEXT( "Menu.Background" ) ) )
        .Content()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .HAlign( HAlign_Center )
            .VAlign( VAlign_Center )
            [
                SAssignNew( VerticalBox, SVerticalBox )
            ]
        ]
    ];

    for ( int32 AssetNameIdx = 0, AssetNameNum = AvailableAssetNames.Num(); AssetNameIdx < AssetNameNum; ++AssetNameIdx )
    {
        FString AssetNameString = TEXT( "" );
        HAPI_StringHandle AssetName = AvailableAssetNames[ AssetNameIdx ];

        FHoudiniEngineString HoudiniEngineString( AssetName );
        if ( HoudiniEngineString.ToFString( AssetNameString ) )
        {
            bIsValidWidget = true;
            FText AssetNameStringText = FText::FromString( AssetNameString );

            VerticalBox->AddSlot()
            .HAlign( HAlign_Center )
            .AutoHeight()
            [
                SNew( SHorizontalBox )
                +SHorizontalBox::Slot()
                .HAlign( HAlign_Center )
                .VAlign( VAlign_Center )
                .Padding( 2.0f, 4.0f )
                [
                    SNew( SButton )
                    .VAlign( VAlign_Center )
                    .HAlign( HAlign_Center )
                    .OnClicked( this, &SAssetSelectionWidget::OnButtonAssetPick, AssetName )
                    .Text( AssetNameStringText )
                    .ToolTipText( AssetNameStringText )
                ]
            ];
        }
    }
}

FReply
SAssetSelectionWidget::OnButtonAssetPick( int32 AssetName )
{
    SelectedAssetName = AssetName;

    WidgetWindow->HideWindow();
    WidgetWindow->RequestDestroyWindow();

    return FReply::Handled();
}

FReply
SAssetSelectionWidget::OnButtonOk()
{
    WidgetWindow->HideWindow();
    WidgetWindow->RequestDestroyWindow();

    return FReply::Handled();
}

FReply
SAssetSelectionWidget::OnButtonCancel()
{
    bIsCancelled = true;

    WidgetWindow->HideWindow();
    WidgetWindow->RequestDestroyWindow();

    return FReply::Handled();
}

#endif


// Macro to update given property on all components.
#define HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( COMPONENT_CLASS, PROPERTY ) \
    do \
    { \
        TArray< UActorComponent * > ReregisterComponents; \
        TArray< USceneComponent * > LocalAttachChildren;\
        GetChildrenComponents(true, LocalAttachChildren); \
        for ( TArray< USceneComponent * >::TConstIterator Iter( LocalAttachChildren ); Iter; ++Iter ) \
        { \
            COMPONENT_CLASS * Component = Cast< COMPONENT_CLASS >( *Iter ); \
            if ( Component ) \
            { \
                Component->PROPERTY = PROPERTY; \
                ReregisterComponents.Add( Component ); \
            } \
        } \
    \
        if ( ReregisterComponents.Num() > 0 ) \
        { \
            FMultiComponentReregisterContext MultiComponentReregisterContext( ReregisterComponents ); \
        } \
    } \
    while( 0 )

bool
UHoudiniAssetComponent::bDisplayEngineNotInitialized = true;

bool
UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch = true;

UHoudiniAssetComponent::UHoudiniAssetComponent( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{
    HoudiniAsset = nullptr;
    bManualRecookRequested = false;
    PreviousTransactionHoudiniAsset = nullptr;
    HoudiniAssetComponentMaterials = nullptr;
#if WITH_EDITOR
    CopiedHoudiniComponent = nullptr;
#endif
    AssetId = -1;
    GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    TransformScaleFactor = HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
    ImportAxis = HRSAI_Unreal;
    HapiNotificationStarted = 0.0;
    AssetCookCount = 0;
    HoudiniAssetComponentTransientFlagsPacked = 0u;

    /** Component flags. **/
    HoudiniAssetComponentFlagsPacked = 0u;
    bEnableCooking = true;
    bUploadTransformsToHoudiniEngine = true;
    bUseHoudiniMaterials = true;
    bCookingTriggersDownstreamCooks = true;

    UObject * Object = ObjectInitializer.GetObj();
    UObject * ObjectOuter = Object->GetOuter();

    if ( ObjectOuter->IsA( AHoudiniAssetActor::StaticClass() ) )
        bIsNativeComponent = true;

    // Set scaling information.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings )
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        TransformScaleFactor = HoudiniRuntimeSettings->TransformScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;
    }

    // Set component properties.
    Mobility = EComponentMobility::Static;
    PrimaryComponentTick.bCanEverTick = true;
    bTickInEditor = true;
    SetGenerateOverlapEvents(false);

    // Similar to UMeshComponent.
    CastShadow = true;
    bUseAsOccluder = true;
    bCanEverAffectNavigation = true;

    // This component requires render update.
    bNeverNeedsRenderUpdate = false;

    // Initialize static mesh generation parameters.
    bGeneratedDoubleSidedGeometry = false;
    GeneratedPhysMaterial = nullptr;
    DefaultBodyInstance.SetCollisionProfileName("BlockAll");
    GeneratedCollisionTraceFlag = CTF_UseDefault;
    GeneratedLpvBiasMultiplier = 1.0f;
    GeneratedLightMapResolution = 32;
    GeneratedLightMapCoordinateIndex = 1;
    bGeneratedUseMaximumStreamingTexelRatio = false;
    GeneratedStreamingDistanceMultiplier = 1.0f;
    GeneratedDistanceFieldResolutionScale = 0.0f;

    bNeedToUpdateNavigationSystem = false;

    // Make an invalid GUID, since we do not have any cooking requests.
    HapiGUID.Invalidate();

    // Create unique component GUID.
    ComponentGUID = FGuid::NewGuid();

    bEditorPropertiesNeedFullUpdate = true;

    bFullyLoaded = false;

    Bounds = FBox( ForceInitToZero );

    DefaultPresetBuffer.Empty();
    PresetBuffer.Empty();
    Parameters.Empty();
    ParameterByName.Empty();
    BakeNameOverrides.Empty();
    DownstreamAssetConnections.Empty();
}

UHoudiniAssetComponent::~UHoudiniAssetComponent()
{}

void
UHoudiniAssetComponent::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetComponent * HoudiniAssetComponent = Cast< UHoudiniAssetComponent >( InThis );

    if ( HoudiniAssetComponent && !HoudiniAssetComponent->IsPendingKill() )
    {
        // Add references for all parameters.
        for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator
            IterParams( HoudiniAssetComponent->Parameters ); IterParams; ++IterParams )
        {
            UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
            if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsPendingKill() )
                Collector.AddReferencedObject( HoudiniAssetParameter, InThis );
        }

        // Add references to all inputs.
        for ( TArray< UHoudiniAssetInput * >::TIterator
            IterInputs( HoudiniAssetComponent->Inputs ); IterInputs; ++IterInputs )
        {
            UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
            if ( HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill() )
                Collector.AddReferencedObject( HoudiniAssetInput, InThis );
        }

        // Add references to all instance inputs.
        for ( auto& InstanceInput : HoudiniAssetComponent->InstanceInputs)
        {
            if ( !InstanceInput || InstanceInput->IsPendingKill() )
                continue;

            Collector.AddReferencedObject( InstanceInput, InThis );
        }

        // Add references to all handles.
        for ( TMap< FString, UHoudiniHandleComponent * >::TIterator
            IterHandles( HoudiniAssetComponent->HandleComponents ); IterHandles; ++IterHandles )
        {
            UHoudiniHandleComponent * HandleComponent = IterHandles.Value();
            if ( HandleComponent && !HandleComponent->IsPendingKill() )
                Collector.AddReferencedObject( HandleComponent, InThis );
        }

        // Add references to all static meshes and corresponding geo parts.
        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator
            Iter( HoudiniAssetComponent->StaticMeshes ); Iter; ++Iter )
        {
            UStaticMesh * StaticMesh = Iter.Value();
            if ( StaticMesh && StaticMesh->IsValidLowLevel() && !StaticMesh->IsPendingKill() )
                Collector.AddReferencedObject( StaticMesh, InThis );
        }

        // Add references to all static meshes and their static mesh components.
        for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TIterator
            Iter( HoudiniAssetComponent->StaticMeshComponents ); Iter; ++Iter )
        {
            UStaticMesh * StaticMesh = Iter.Key();
            UStaticMeshComponent * StaticMeshComponent = Iter.Value();

            if (!StaticMesh || !StaticMeshComponent->IsValidLowLevel() || StaticMesh->IsPendingKill())
                continue;

            Collector.AddReferencedObject( StaticMesh, InThis );

            if (!StaticMeshComponent || !StaticMeshComponent->IsValidLowLevel() || StaticMeshComponent->IsPendingKill())
                continue;

            Collector.AddReferencedObject( StaticMeshComponent, InThis );
        }

        // Add references to all temporary cooked mesh packages
        for (TMap< FHoudiniGeoPartObject, TWeakObjectPtr<class UPackage> > ::TIterator
            Iter(HoudiniAssetComponent->CookedTemporaryStaticMeshPackages); Iter; ++Iter)
        {
            UPackage * MeshPackage = Iter.Value().Get();
            if (MeshPackage && !MeshPackage->IsPendingKill())
                Collector.AddReferencedObject(MeshPackage, InThis);
        }

        // Add references to all spline components.
        for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* >::TIterator
            Iter( HoudiniAssetComponent->SplineComponents ); Iter; ++Iter )
        {
            UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
            if ( !HoudiniSplineComponent || !HoudiniSplineComponent->IsValidLowLevel() || HoudiniSplineComponent->IsPendingKill() )
                continue;

            Collector.AddReferencedObject( HoudiniSplineComponent, InThis );
        }

        // Add references to all Landscape
        for ( TMap< FHoudiniGeoPartObject, TWeakObjectPtr <ALandscapeProxy> >::TIterator
            Iter( HoudiniAssetComponent->LandscapeComponents ); Iter; ++Iter)
        {
            ALandscapeProxy * HoudiniLandscape = Iter.Value().Get();
            if ( !HoudiniLandscape || HoudiniLandscape->IsPendingKill() || !HoudiniLandscape->IsValidLowLevel() )
                continue;

            Collector.AddReferencedObject( HoudiniLandscape, InThis );
        }

        // Add references to all generated Landscape layer objects
        for ( TMap< TWeakObjectPtr<class UPackage>, FHoudiniGeoPartObject > ::TIterator
            Iter( HoudiniAssetComponent->CookedTemporaryLandscapeLayers ); Iter; ++Iter )
        {
            UPackage * LayerPackage = Iter.Key().Get();
            if ( LayerPackage && !LayerPackage->IsPendingKill() )
                Collector.AddReferencedObject( LayerPackage, InThis );
        }

        // Retrieve asset associated with this component and add reference to it.
        // Also do not add reference if it is being referenced by preview component.
        UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
        if ( HoudiniAsset && !HoudiniAsset->IsPendingKill() && !HoudiniAssetComponent->bIsPreviewComponent )
        {
            // Manually add a reference to Houdini asset from this component.
            Collector.AddReferencedObject( HoudiniAsset, InThis );
        }

        // Add reference to material replacements.
        UHoudiniAssetComponentMaterials* HoudiniAssetComponentMaterials = HoudiniAssetComponent->HoudiniAssetComponentMaterials;
        if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
            Collector.AddReferencedObject( HoudiniAssetComponent->HoudiniAssetComponentMaterials, InThis );

        // Add references to all temporary cooked material packages
        for (TMap< FString, TWeakObjectPtr<class UPackage> > ::TIterator
            Iter(HoudiniAssetComponent->CookedTemporaryPackages); Iter; ++Iter)
        {
            UPackage * MaterialPackage = Iter.Value().Get();
            if (MaterialPackage && !MaterialPackage->IsPendingKill())
                Collector.AddReferencedObject(MaterialPackage, InThis);
        }

    }

    // Call base implementation.
    Super::AddReferencedObjects( InThis, Collector );
}

void
UHoudiniAssetComponent::SetNative( bool InbIsNativeComponent )
{
    bIsNativeComponent = InbIsNativeComponent;
}

HAPI_NodeId
UHoudiniAssetComponent::GetAssetId() const
{
    return AssetId;
}

void
UHoudiniAssetComponent::SetAssetId( HAPI_NodeId InAssetId )
{
    AssetId = InAssetId;
}

bool
UHoudiniAssetComponent::HasValidAssetId() const
{
    return FHoudiniEngineUtils::IsHoudiniNodeValid( AssetId );
}

bool
UHoudiniAssetComponent::IsComponentValid() const
{
    if( !IsValidLowLevel() )
        return false;

    if ( IsTemplate() )
        return false;

    if ( IsPendingKillOrUnreachable() )
        return false;

    if ( !GetOuter() ) //|| !GetOuter()->GetLevel() )
        return false;

    return true;
}

UHoudiniAsset *
UHoudiniAssetComponent::GetHoudiniAsset() const
{
    return HoudiniAsset;
}


AHoudiniAssetActor*
UHoudiniAssetComponent::GetHoudiniAssetActorOwner() const
{
    return Cast< AHoudiniAssetActor >( GetOwner() );
}

void
UHoudiniAssetComponent::SetHoudiniAsset( UHoudiniAsset * InHoudiniAsset )
{
    // If it is the same asset, do nothing.
    if ( InHoudiniAsset == HoudiniAsset )
        return;

    UHoudiniAsset * Asset = nullptr;
    AHoudiniAssetActor * HoudiniAssetActor = Cast< AHoudiniAssetActor >( GetOwner() );

    HoudiniAsset = InHoudiniAsset;

    // Reset material tracking.
    if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
        HoudiniAssetComponentMaterials->ResetMaterialInfo();

    if ( !bIsNativeComponent )
        return;

#if WITH_EDITOR

    // Houdini Asset has been changed, we need to reset corresponding HDA and relevant resources.
    ResetHoudiniResources();

#endif

    // Clear all created parameters.
    ClearParameters();

    // Clear all inputs.
    ClearInputs();

    // Clear all instance inputs.
    ClearInstanceInputs();

    // Release all curve related resources.
    ClearCurves();

    // Clear all handles.
    ClearHandles();

    // Clear all landscapes.
    ClearLandscapes();

    // Set Houdini logo to be default geometry.
    ReleaseObjectGeoPartResources( StaticMeshes );
    StaticMeshes.Empty();
    StaticMeshComponents.Empty();
    CreateStaticMeshHoudiniLogoResource( StaticMeshes );

    bIsPreviewComponent = false;
    if ( !InHoudiniAsset )
    {
#if WITH_EDITOR
        UpdateEditorProperties( false );
#endif
        return;
    }

    if ( HoudiniAssetActor )
        bIsPreviewComponent = HoudiniAssetActor->IsUsedForPreview();

    if( !bIsNativeComponent )
        bLoadedComponent = false;

    // Get instance of Houdini Engine.
    FHoudiniEngine & HoudiniEngine = FHoudiniEngine::Get();

#if WITH_EDITOR

    if ( !bIsPreviewComponent )
    {
        // If the fist session on the engine was never initialized, do it now
        if (!FHoudiniEngine::Get().GetFirstSessionCreated())
        {
            FHoudiniEngine::Get().RestartSession();
        }

        if ( HoudiniEngine.IsInitialized() )
        {
            if ( !bLoadedComponent )
            {
                // If component is not a loaded component, instantiate and start ticking.
                StartTaskAssetInstantiation( false, true );
            }
            else if ( bTransactionAssetChange )
            {
                // If assigned asset is transacted asset, instantiate and start ticking. Also treat as loaded component.
                StartTaskAssetInstantiation( true, true );
            }
        }
        else
        {
            if ( UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch && HoudiniEngine.CheckHapiVersionMismatch() )
            {
                // We have mismatch in defined and running versions.
                int32 RunningEngineMajor = 0;
                int32 RunningEngineMinor = 0;
                int32 RunningEngineApi = 0;

                const HAPI_Session * Session = FHoudiniEngine::Get().GetSession();

                // Retrieve version numbers for running Houdini Engine.
                FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor );
                FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor );
                FHoudiniApi::GetEnvInt( HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi );

                // Get platform specific name of libHAPI.
                FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

                // Some sanity checks.
                int32 BuiltEngineMajor = FMath::Max( 0, HAPI_VERSION_HOUDINI_ENGINE_MAJOR );
                int32 BuiltEngineMinor = FMath::Max( 0, HAPI_VERSION_HOUDINI_ENGINE_MINOR );
                int32 BuiltEngineApi = FMath::Max( 0, HAPI_VERSION_HOUDINI_ENGINE_API );

                FString WarningMessage = FString::Printf(
                    TEXT( "Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d mismatch. " )
                    TEXT( "%s was loaded, but has wrong version. " )
                    TEXT( "No cooking / instantiation will take place." ),
                    BuiltEngineMajor,
                    BuiltEngineMinor,
                    BuiltEngineApi,
                    RunningEngineMajor,
                    RunningEngineMinor,
                    RunningEngineApi,
                    *LibHAPIName );

                FString WarningTitle = TEXT( "Houdini Engine Plugin Warning" );
                FText WarningTitleText = FText::FromString( WarningTitle );
                FMessageDialog::Debugf( FText::FromString( WarningMessage ), &WarningTitleText );

                UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch = false;
            }
            else if ( UHoudiniAssetComponent::bDisplayEngineNotInitialized )
            {
                // Get platform specific name of libHAPI.
                FString LibHAPIName = FHoudiniEngineUtils::HoudiniGetLibHAPIName();

                // If this is first time component is instantiated and we do not have Houdini Engine initialized.
                FString WarningTitle = TEXT( "Houdini Engine Plugin Warning" );
                FText WarningTitleText = FText::FromString( WarningTitle );
                FString WarningMessage = FString::Printf(
                    TEXT( "Houdini Installation was not detected." )
                    TEXT( "Failed to locate or load %s. " )
                    TEXT( "No cooking / instantiation will take place." ),
                    *LibHAPIName );

                FMessageDialog::Debugf( FText::FromString( WarningMessage ), &WarningTitleText );

                UHoudiniAssetComponent::bDisplayEngineNotInitialized = false;
            }
        }
    }

#endif
}

void
UHoudiniAssetComponent::AddDownstreamAsset( UHoudiniAssetComponent * InDownstreamAssetComponent, int32 InInputIndex )
{
    if ( InDownstreamAssetComponent && !InDownstreamAssetComponent->IsPendingKill() )
    {
        if ( DownstreamAssetConnections.Contains( InDownstreamAssetComponent ) )
        {
            TSet< int32 > & InputIndicesSet = DownstreamAssetConnections[ InDownstreamAssetComponent ];
            InputIndicesSet.Add( InInputIndex );
        }
        else
        {
            TSet< int32 > InputIndicesSet;
            InputIndicesSet.Add( InInputIndex );
            DownstreamAssetConnections.Add( InDownstreamAssetComponent, InputIndicesSet );
        }
    }
}

void
UHoudiniAssetComponent::RemoveDownstreamAsset( UHoudiniAssetComponent * InDownstreamAssetComponent, int32 InInputIndex )
{
    if ( !InDownstreamAssetComponent || InDownstreamAssetComponent->IsPendingKill() )
        return;

    if ( DownstreamAssetConnections.Contains( InDownstreamAssetComponent ) )
    {
        TSet< int32 > & InputIndicesSet = DownstreamAssetConnections[ InDownstreamAssetComponent ];
        if ( InputIndicesSet.Contains( InInputIndex ) )
            InputIndicesSet.Remove( InInputIndex );
    }
}

void
UHoudiniAssetComponent::CreateObjectGeoPartResources(
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshMap )
{
    // Reset Houdini logo flag.
    bContainsHoudiniLogoGeometry = false;
    
    // We need to store instancers as they need to be processed after all other meshes.
    TArray< FHoudiniGeoPartObject > FoundInstancers;
    TArray< FHoudiniGeoPartObject > FoundCurves;
    TArray< FHoudiniGeoPartObject > FoundVolumes;
    TMap< FHoudiniGeoPartObject, UStaticMesh* > StaleParts;

    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshMap ); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
        UStaticMesh * StaticMesh = Iter.Value();

        if ( HoudiniGeoPartObject.IsInstancer() )
        {
            FoundInstancers.Add( HoudiniGeoPartObject );
        }
        else if ( HoudiniGeoPartObject.IsPackedPrimitiveInstancer() )
        {
            // Packed Primitives should be processed before other instancer in case they are instanced by the same
            FoundInstancers.Insert( HoudiniGeoPartObject, 0 );
        }
        else if ( HoudiniGeoPartObject.IsCurve() )
        {
            // This geo part is a curve and has no mesh assigned.
            check( !StaticMesh );
            FoundCurves.Add( HoudiniGeoPartObject );
        }
        else if ( HoudiniGeoPartObject.IsVolume() )
        {
            FoundVolumes.Add( HoudiniGeoPartObject );
        }
        else
        {
            // This geo part is visible and not an instancer and must have static mesh assigned.
            if ( HoudiniGeoPartObject.IsVisible() && ( !StaticMesh || StaticMesh->IsPendingKill() ) )
            {
                HOUDINI_LOG_WARNING( TEXT( "No static mesh generated for visible part %d,%d,%d" ), HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.PartId );
                continue;
            }

            UStaticMeshComponent * StaticMeshComponent = nullptr;
            UStaticMeshComponent * FoundStaticMeshComponent = LocateStaticMeshComponent( StaticMesh );

            if ( FoundStaticMeshComponent && !FoundStaticMeshComponent->IsPendingKill() )
            {
                StaticMeshComponent = FoundStaticMeshComponent;
                if ( ! HoudiniGeoPartObject.IsVisible() )
                {
                    // We have a mesh and component for a part which is invisible.
                    // Visibility may have changed since last cook
                    StaleParts.Add( HoudiniGeoPartObject, StaticMesh );
                    continue;
                }
            }
            else if ( HoudiniGeoPartObject.IsVisible() )
            {
                // Create necessary component.
                StaticMeshComponent = NewObject< UStaticMeshComponent >(
                    GetOwner() ? GetOwner() : GetOuter(), UStaticMeshComponent::StaticClass(),
                    NAME_None, RF_Transactional );

                if ( StaticMeshComponent && !StaticMeshComponent->IsPendingKill() )
                {
                    StaticMeshComponent->SetStaticMesh(StaticMesh);
                    StaticMeshComponent->SetVisibility(true);
                    StaticMeshComponent->SetMobility(Mobility);
                    StaticMeshComponent->RegisterComponent();

                    // Property propagation: set the new SMC's properties to the HAC's current settings
                    CopyComponentPropertiesTo(StaticMeshComponent);

                    // Attach created static mesh component to our Houdini component.
                    StaticMeshComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

                    // Add to the map of components.
                    StaticMeshComponents.Add(StaticMesh, StaticMeshComponent);
                }
            }

            if ( StaticMeshComponent && !StaticMeshComponent->IsPendingKill())
            {
                // If this is a collision geo, we need to make it invisible.
                if (HoudiniGeoPartObject.IsCollidable())
                {
                    StaticMeshComponent->SetVisibility( false );
                    StaticMeshComponent->SetHiddenInGame( true );
                    StaticMeshComponent->SetCollisionProfileName( FName( TEXT( "InvisibleWall" ) ) );
                    StaticMeshComponent->SetCastShadow(false);
                }
                else
                {
                    // Visibility may have changed so we still need to update it
                    StaticMeshComponent->SetVisibility( HoudiniGeoPartObject.IsVisible() );
                    StaticMeshComponent->SetHiddenInGame( !HoudiniGeoPartObject.IsVisible() );
                }

                // And we will need to update the navmesh later
                if( HoudiniGeoPartObject.IsCollidable() || HoudiniGeoPartObject.IsRenderCollidable() )
                    bNeedToUpdateNavigationSystem = true;

                // Transform the component by transformation provided by HAPI.
                StaticMeshComponent->SetRelativeTransform( HoudiniGeoPartObject.TransformMatrix );

                // If the static mesh had sockets, we can assign the desired actor to them now
                int32 NumberOfSockets = StaticMesh == nullptr ? 0 : StaticMesh->Sockets.Num();
                for( int32 nSocket = 0; nSocket < NumberOfSockets; nSocket++ )
                {
                    UStaticMeshSocket* MeshSocket = StaticMesh->Sockets[ nSocket ];
                    if ( MeshSocket && !MeshSocket->IsPendingKill() && ( MeshSocket->Tag.IsEmpty() ) )
                        continue;

                    FHoudiniEngineUtils::AddActorsToMeshSocket( StaticMesh->Sockets[ nSocket ], StaticMeshComponent );
                }

                // Try to update uproperty atributes
                // No need to update uprops if we've not yet been instanced
                if ( bFullyLoaded )
                    FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( StaticMeshComponent, HoudiniGeoPartObject );
            }
        }
    }

    if ( StaleParts.Num() )
    {
        for ( auto Iter : StaleParts )
        {
            StaticMeshMap.Remove( Iter.Key );
        }
        ReleaseObjectGeoPartResources( StaleParts, true );
    }

    // Skip self assignment.
    if ( &StaticMeshes != &StaticMeshMap )
        StaticMeshes = StaticMeshMap;

#if WITH_EDITOR
    if ( FHoudiniEngineUtils::IsHoudiniNodeValid( AssetId ) )
    {
        // Create necessary instance inputs.
        CreateInstanceInputs( FoundInstancers );

        // Create necessary curves.
        CreateCurves( FoundCurves );

        // Create necessary landscapes
        CreateAllLandscapes( FoundVolumes );
    }
#endif

    CleanUpAttachedStaticMeshComponents();

    // Now that all the Meshes/Landscapes are created, see if we need to create material instances from attributes
    CreateOrUpdateMaterialInstances();

    // Mobility will be set to static if we generated a Landscape,
    // Or to movable if any of our children is movable
    UpdateMobility();
}


void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources( bool bDeletePackages )
{
    ReleaseObjectGeoPartResources( StaticMeshes, bDeletePackages );
}

void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources(
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshMap,
    bool bDeletePackages,
    TMap< FHoudiniGeoPartObject, UStaticMesh * > * pKeepIfContainedIn )
{
    // Record generated static meshes which we need to delete.
    TArray< UStaticMesh * > StaticMeshesToDelete;

    // Get Houdini logo.
    UStaticMesh * HoudiniLogoMesh = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();

    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshMap ); Iter; ++Iter )
    {
        // If we find the new mesh in the existing list, don't destroy the existing static mesh slot
        if ( pKeepIfContainedIn )
        {
            const FHoudiniGeoPartObject& GeoPart = Iter.Key();
            if (LocateStaticMeshByNames(GeoPart, *pKeepIfContainedIn))
            {
                // The old static mesh was found in the new list, no need to destroy it
                continue; 
            }
        }

        UStaticMesh * StaticMesh = Iter.Value();
        if ( !StaticMesh || StaticMesh->IsPendingKill() )
            continue;

        // Removes the static mesh component from the map, detaches and destroys it.
        RemoveStaticMeshComponent( StaticMesh );

        if ( bDeletePackages && ( StaticMesh != HoudiniLogoMesh ) )
        {
            // Make sure this static mesh is not referenced.
            UObject * ObjectMesh = (UObject *) StaticMesh;
            FReferencerInformationList Referencers;

            // Check if object is referenced and get its referencers, if it is.
            bool bReferenced = IsReferenced(
                ObjectMesh, GARBAGE_COLLECTION_KEEPFLAGS,
                EInternalObjectFlags::GarbageCollectionKeepFlags, true, &Referencers );

            if ( !bReferenced || IsObjectReferencedLocally( StaticMesh, Referencers ) )
            {
                // Only delete generated mesh if it has not been saved manually.
                UPackage * Package = Cast< UPackage >( StaticMesh->GetOuter() );
                if ( !Package || !Package->bHasBeenFullyLoaded )
                    StaticMeshesToDelete.Add(StaticMesh);
            }
        }
    }
    
    // CleanUpAttachedStaticMeshComponents();

    // Remove unused meshes.
    StaticMeshMap.Empty();

#if WITH_EDITOR    // Delete no longer used generated static meshes.
    int32 MeshNum = StaticMeshesToDelete.Num();

    if ( bDeletePackages && MeshNum > 0 )
    {
        for ( int32 MeshIdx = 0; MeshIdx < MeshNum; ++MeshIdx )
        {
            UStaticMesh * StaticMesh = StaticMeshesToDelete[ MeshIdx ];

            // Free any RHI resources.
            StaticMesh->PreEditChange( nullptr );
            StaticMesh->MarkPendingKill();

            ObjectTools::DeleteSingleObject( StaticMesh, false );
        }
    }

#endif
}

void
UHoudiniAssetComponent::CleanUpAttachedStaticMeshComponents()
{
    // Record generated static meshes which we need to delete.
    TArray< UStaticMesh * > StaticMeshesToDelete;

    // Collect all the static mesh component for this asset
    TMap<const UStaticMeshComponent *, FHoudiniGeoPartObject> AllSMC = CollectAllStaticMeshComponents();
    
    // We'll check all the children static mesh components for junk
    const auto & LocalAttachChildren = GetAttachChildren();
    for (TArray< USceneComponent * >::TConstIterator Iter( LocalAttachChildren ); Iter; ++Iter)
    {
        UStaticMeshComponent * StaticMeshComponent = Cast< UStaticMeshComponent >(*Iter);
        if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
            continue;

        bool bNeedToCleanMeshComponent = false;
        UStaticMesh * StaticMesh = StaticMeshComponent->GetStaticMesh();

        if (AllSMC.Find(StaticMeshComponent) == nullptr)
            bNeedToCleanMeshComponent = true;
        
        // Do not clean up component attached to a socket
        if ( StaticMeshComponent->GetAttachSocketName() != NAME_None )
            bNeedToCleanMeshComponent = false;

        if ( bNeedToCleanMeshComponent )
        {
            // This StaticMeshComponent is attached to the asset but not in the map, and not an instance.
            // It may be a leftover from previous cook/undo/redo and needs to be properly destroyed
            StaticMeshComponent->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
            StaticMeshComponent->UnregisterComponent();
            StaticMeshComponent->DestroyComponent();

            StaticMeshesToDelete.Add( StaticMesh );

            //HOUDINI_LOG_WARNING( TEXT("CLEANUP: Deleted extra Static Mesh Component for %s"), *(StaticMesh->GetName()) );
        }
    }

#if WITH_EDITOR
    // We'll try to delete the undesirable static meshes too
    for (int32 MeshIdx = 0; MeshIdx < StaticMeshesToDelete.Num(); ++MeshIdx)
    {
        UStaticMesh * StaticMesh = StaticMeshesToDelete[MeshIdx];
        if ( !StaticMesh || StaticMesh->IsPendingKill() )
            continue;
                
        UObject * ObjectMesh = (UObject *)StaticMesh;
        if ( ObjectMesh->IsUnreachable() )
            continue;

        // Check if object is referenced and get its referencers, if it is.
        FReferencerInformationList Referencers; 
        bool bReferenced = IsReferenced(
            ObjectMesh, GARBAGE_COLLECTION_KEEPFLAGS,
            EInternalObjectFlags::GarbageCollectionKeepFlags, true, &Referencers);

        if ( !bReferenced || IsObjectReferencedLocally( StaticMesh, Referencers ) )
        {
            // Only delete generated mesh if it has not been saved manually.
            UPackage * Package = Cast< UPackage >( StaticMesh->GetOuter() );
            if ( !Package || !Package->bHasBeenFullyLoaded )
            {
                StaticMesh->PreEditChange( nullptr );
                ObjectTools::DeleteSingleObject( StaticMesh, false );
            }
        }
    }
#endif
}

bool
UHoudiniAssetComponent::IsObjectReferencedLocally(
    UStaticMesh * StaticMesh,
    FReferencerInformationList & Referencers ) const
{
    if ( Referencers.InternalReferences.Num() == 0 && Referencers.ExternalReferences.Num() == 1 )
    {
        const FReferencerInformation & ExternalReferencer = Referencers.ExternalReferences[ 0 ];
        if ( ExternalReferencer.Referencer == this )
        {
            // Only referencer is this component.
            return true;
        }
    }

    return false;
}

void
UHoudiniAssetComponent::CollectSubstanceParameters( TMap< FString, UHoudiniAssetParameter * > & SubstanceParameters ) const
{
    SubstanceParameters.Empty();

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TConstIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if  (!HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill() )
            continue;

        if ( HoudiniAssetParameter->IsSubstanceParameter() )
            SubstanceParameters.Add( HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter );
    }
}

void
UHoudiniAssetComponent::CollectAllParametersOfType(
    UClass * ParameterClass,
    TMap< FString, UHoudiniAssetParameter * > & ClassParameters ) const
{
    ClassParameters.Empty();

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TConstIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if (!HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill())
            continue;

        if ( HoudiniAssetParameter->IsA( ParameterClass ) )
            ClassParameters.Add( HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter );
    }
}

UHoudiniAssetParameter *
UHoudiniAssetComponent::FindParameter( const FString & ParameterName ) const
{
    UHoudiniAssetParameter * const * FoundHoudiniAssetParameter = ParameterByName.Find( ParameterName );
    UHoudiniAssetParameter * HoudiniAssetParameter = nullptr;

    if ( FoundHoudiniAssetParameter )
        HoudiniAssetParameter = *FoundHoudiniAssetParameter;

    if ( !HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill() )
        return nullptr;

    return HoudiniAssetParameter;
}

void
UHoudiniAssetComponent::GetInputs( TArray< UHoudiniAssetInput* >& AllInputs, bool IncludeObjectPathParameter )
{
    AllInputs.Empty();

    for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( Inputs ); IterInputs; ++IterInputs )
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if ( !HoudiniAssetInput || HoudiniAssetInput->IsPendingKill() )
            continue;

        AllInputs.Add( HoudiniAssetInput );
    }

    if ( !IncludeObjectPathParameter )
        return;

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TConstIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetInput* ObjectPathInput = Cast< UHoudiniAssetInput >( IterParams.Value() );
        if ( !ObjectPathInput || ObjectPathInput->IsPendingKill() )
            continue;

        AllInputs.Add ( ObjectPathInput );
    }
}

void
UHoudiniAssetComponent::GetAllUsedStaticMeshes( TArray< UStaticMesh * > & UsedStaticMeshes )
{
    UsedStaticMeshes.Empty();

    // Add all static meshes.
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshes ); Iter; ++Iter )
    {
        UStaticMesh * StaticMesh = Iter.Value();
        if ( StaticMesh && !StaticMesh->IsPendingKill() )
            UsedStaticMeshes.Add( StaticMesh );
    }
}

TMap<const UStaticMeshComponent *, FHoudiniGeoPartObject>
UHoudiniAssetComponent::CollectAllStaticMeshComponents() const
{
    TMap<const UStaticMeshComponent *, FHoudiniGeoPartObject> OutSMComponentToPart;

    // Add all the instance meshes, including the variations
    for ( const UHoudiniAssetInstanceInput* InstanceInput : InstanceInputs )
    {
        if ( !InstanceInput || InstanceInput->IsPendingKill() )
            continue;

        for ( const UHoudiniAssetInstanceInputField* InputField : InstanceInput->GetInstanceInputFields() )
        {
            if ( !InputField || InputField->IsPendingKill() )
                continue;

            for ( int32 VarIndex = 0; VarIndex < InputField->InstanceVariationCount(); ++VarIndex )
            {
                UObject* Comp = InputField->GetInstancedComponent( VarIndex );
                if ( Comp && InputField->GetInstanceVariation( VarIndex ) && Comp->IsA<UInstancedStaticMeshComponent>() )
                {
                    OutSMComponentToPart.Add( Cast<UInstancedStaticMeshComponent>( Comp ), InputField->GetHoudiniGeoPartObject() );
                }
            }
        }
    }

    // add all the plain UStaticMeshComponent
    for ( const auto& MeshPart : GetStaticMeshes() )
    {
        if ( UStaticMeshComponent * SMC = LocateStaticMeshComponent( MeshPart.Value ) )
        {
            OutSMComponentToPart.Add( SMC, MeshPart.Key );
        }
    }

    return OutSMComponentToPart;
}

TMap<const UHoudiniInstancedActorComponent *, FHoudiniGeoPartObject>
UHoudiniAssetComponent::CollectAllInstancedActorComponents() const
{
    TMap<const UHoudiniInstancedActorComponent *, FHoudiniGeoPartObject> OutSMComponentToPart;
    for ( const UHoudiniAssetInstanceInput* InstanceInput : InstanceInputs )
    {
        if ( !InstanceInput || InstanceInput->IsPendingKill() )
            continue;

        for ( const UHoudiniAssetInstanceInputField* InputField : InstanceInput->GetInstanceInputFields() )
        {
            if ( !InputField || InputField->IsPendingKill() )
                continue;

            for ( int32 VarIndex = 0; VarIndex < InputField->InstanceVariationCount(); ++VarIndex )
            {
                UObject* Comp = InputField->GetInstancedComponent( VarIndex );
                if (!Comp || Comp->IsPendingKill())
                    continue;

                if ( InputField->GetInstanceVariation( VarIndex ) && Comp->IsA<UHoudiniInstancedActorComponent>() )
                {
                    OutSMComponentToPart.Add( Cast<UHoudiniInstancedActorComponent>( Comp ), InputField->GetHoudiniGeoPartObject() );
                }
            }
        }
    }
    return OutSMComponentToPart;
}

TMap<const UHoudiniMeshSplitInstancerComponent *, FHoudiniGeoPartObject>
UHoudiniAssetComponent::CollectAllMeshSplitInstancerComponents() const
{
    TMap<const UHoudiniMeshSplitInstancerComponent *, FHoudiniGeoPartObject> OutSMComponentToPart;
    for( const UHoudiniAssetInstanceInput* InstanceInput : InstanceInputs )
    {
        if (!InstanceInput || InstanceInput->IsPendingKill())
            continue;

        for( const UHoudiniAssetInstanceInputField* InputField : InstanceInput->GetInstanceInputFields() )
        {
            if ( !InputField || InputField->IsPendingKill() )
                continue;

            for( int32 VarIndex = 0; VarIndex < InputField->InstanceVariationCount(); ++VarIndex )
            {
                UObject* Comp = InputField->GetInstancedComponent(VarIndex);
                if (!Comp || Comp->IsPendingKill())
                    continue;

                if( InputField->GetInstanceVariation(VarIndex) && Comp->IsA<UHoudiniMeshSplitInstancerComponent>() )
                {
                    OutSMComponentToPart.Add(Cast<UHoudiniMeshSplitInstancerComponent>(Comp), InputField->GetHoudiniGeoPartObject());
                }
            }
        }
    }
    return OutSMComponentToPart;
}

bool
UHoudiniAssetComponent::CheckGlobalSettingScaleFactors() const
{
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings )
    {
        return ( GeneratedGeometryScaleFactor == HoudiniRuntimeSettings->GeneratedGeometryScaleFactor &&
                TransformScaleFactor == HoudiniRuntimeSettings->TransformScaleFactor );
    }

    return ( GeneratedGeometryScaleFactor == HAPI_UNREAL_SCALE_FACTOR_POSITION &&
            TransformScaleFactor == HAPI_UNREAL_SCALE_FACTOR_TRANSLATION );
}

#if WITH_EDITOR

bool
UHoudiniAssetComponent::IsInstantiatingOrCooking() const
{
    return HapiGUID.IsValid();
}

bool
UHoudiniAssetComponent::HasBeenInstantiatedButNotCooked() const
{
    return ( FHoudiniEngineUtils::IsHoudiniNodeValid( AssetId ) && ( 0 == AssetCookCount ) );
}

void
UHoudiniAssetComponent::AssignUniqueActorLabel()
{
    if ( GEditor && FHoudiniEngineUtils::IsValidNodeId( AssetId ) )
    {
        AHoudiniAssetActor * HoudiniAssetActor = GetHoudiniAssetActorOwner();
        if ( HoudiniAssetActor )
        {
            FString UniqueName;
            if ( FHoudiniEngineUtils::GetHoudiniAssetName( AssetId, UniqueName ) )
                FActorLabelUtilities::SetActorLabelUnique( HoudiniAssetActor, UniqueName );
        }
    }
}

void
UHoudiniAssetComponent::StartHoudiniUIUpdateTicking()
{
    // If we have no timer delegate spawned for ui update, spawn one.
    if ( !TimerDelegateUIUpdate.IsBound() && GEditor )
    {
        TimerDelegateUIUpdate = FTimerDelegate::CreateUObject( this, &UHoudiniAssetComponent::TickHoudiniUIUpdate );

        // We need to register delegate with the timer system.
        static const float TickTimerDelay = 0.25f;
        GEditor->GetTimerManager()->SetTimer( TimerHandleUIUpdate, TimerDelegateUIUpdate, TickTimerDelay, true );
    }
}

void
UHoudiniAssetComponent::StopHoudiniUIUpdateTicking()
{
    if ( TimerDelegateUIUpdate.IsBound() && GEditor )
    {
        GEditor->GetTimerManager()->ClearTimer( TimerHandleUIUpdate );
        TimerDelegateUIUpdate.Unbind();
    }
}

void
UHoudiniAssetComponent::TickHoudiniUIUpdate()
{
    UpdateEditorProperties( true );
}

void
UHoudiniAssetComponent::StartHoudiniTicking()
{
    // If we have no timer delegate spawned for this component, spawn one.
    if ( !TimerDelegateCooking.IsBound() && GEditor )
    {
        TimerDelegateCooking = FTimerDelegate::CreateUObject( this, &UHoudiniAssetComponent::TickHoudiniComponent );

        // We need to register delegate with the timer system.
        static const float TickTimerDelay = 0.25f;
        GEditor->GetTimerManager()->SetTimer( TimerHandleCooking, TimerDelegateCooking, TickTimerDelay, true );

        // Grab current time for delayed notification.
        HapiNotificationStarted = FPlatformTime::Seconds();
    }

    // If the fist session of houdini engine hasnt been initialized yet, do it now
    if (!FHoudiniEngine::Get().GetFirstSessionCreated())
    {
        FHoudiniEngine::Get().RestartSession();
    }
}

void
UHoudiniAssetComponent::StopHoudiniTicking()
{
    if ( TimerDelegateCooking.IsBound() && GEditor )
    {
        GEditor->GetTimerManager()->ClearTimer( TimerHandleCooking );
        TimerDelegateCooking.Unbind();

        // Reset time for delayed notification.
        HapiNotificationStarted = 0.0;
    }
}

void
UHoudiniAssetComponent::PostCook( bool bCookError )
{
    // Show busy cursor.
    FScopedBusyCursor ScopedBusyCursor;

    // Create parameters and inputs.
    CreateParameters();
    CreateInputs();
    CreateHandles();

    if ( bCookError )
    {
        // We need to reset the manual recook flag here to avoid endless cooking
        bManualRecookRequested = false;
        return;
    }

    FTransform ComponentTransform;
    TMap< FHoudiniGeoPartObject, UStaticMesh * > NewStaticMeshes;
    
    FHoudiniCookParams HoudiniCookParams( this );
    HoudiniCookParams.StaticMeshBakeMode = FHoudiniCookParams::GetDefaultStaticMeshesCookMode();
    HoudiniCookParams.MaterialAndTextureBakeMode = FHoudiniCookParams::GetDefaultMaterialAndTextureCookMode();

    if ( FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
        GetAssetId(),
        HoudiniCookParams,
        !CheckGlobalSettingScaleFactors(),
        bManualRecookRequested,
        StaticMeshes, 
        NewStaticMeshes, 
        ComponentTransform ) )
    {
        // Remove all duplicates. After this operation, old map will have meshes which we need to deallocate.
        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator
            Iter( NewStaticMeshes ); Iter; ++Iter )
        {
            const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
            UStaticMesh * StaticMesh = Iter.Value();

            // Removes the mesh from previous map of meshes
            UStaticMesh * FoundOldStaticMesh = LocateStaticMesh( HoudiniGeoPartObject );
            if (!FoundOldStaticMesh)
                FoundOldStaticMesh = LocateStaticMeshByNames(HoudiniGeoPartObject, StaticMeshes);

            if ( ( FoundOldStaticMesh ) && ( FoundOldStaticMesh == StaticMesh ) )
            {
                // Mesh has not changed, we need to remove it from the old map to avoid deallocation.
                StaticMeshes.Remove( HoudiniGeoPartObject );
            }

            // See if we need to update the HoudiniAssetComponent uproperties
            FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( this, HoudiniGeoPartObject );
        }

        // Make sure rendering is done
        FlushRenderingCommands();

        // Update the bake folder as it might have been updated by an override
        BakeFolder = HoudiniCookParams.BakeFolder;

        if ( NewStaticMeshes.Num() == 0 && bContainsHoudiniLogoGeometry )
        {
            // We're not creating anything new and are already using the houdini logo
            // No need to destroy then recreate the Houdini Logo
            if ( StaticMeshes.Num() !=  1)
                CreateStaticMeshHoudiniLogoResource( StaticMeshes );
        }
        else
        {
            // Free meshes and components that are no longer used.
            // compare with first array, keep if contained in NewMeshes
            ReleaseObjectGeoPartResources(StaticMeshes, true, &NewStaticMeshes);

            // Set meshes and create new components for those meshes that do not have them.
            if (NewStaticMeshes.Num() > 0)
                CreateObjectGeoPartResources(NewStaticMeshes);
            else
                CreateStaticMeshHoudiniLogoResource(NewStaticMeshes);
        }
    }

    // We can reset the manual recook flag now that the static meshes have been created
    bManualRecookRequested = false;

    // Invoke cooks of downstream assets.
    if ( bCookingTriggersDownstreamCooks )
    {
        for ( TMap<UHoudiniAssetComponent *, TSet< int32 > >::TIterator IterAssets( DownstreamAssetConnections );
            IterAssets;
            ++IterAssets )
        {
            UHoudiniAssetComponent * DownstreamAsset = IterAssets.Key();
            if ( !DownstreamAsset || DownstreamAsset->IsPendingKill() )
                continue;

            DownstreamAsset->bManualRecookRequested = true;
            DownstreamAsset->NotifyParameterChanged( nullptr );
        }
    }
}

void
UHoudiniAssetComponent::TickHoudiniComponent()
{
    // Only tick/cook when in Editor
    // This prevents PIE cooks or runtime cooks due to inputs moving
    if (!GetWorld() || (GetWorld()->WorldType != EWorldType::Editor))
    {
        return;
    }

    // Get settings.
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    FHoudiniEngineTaskInfo TaskInfo;
    bool bStopTicking = false;
    bool bFinishedLoadedInstantiation = false;

    static float NotificationFadeOutDuration = 2.0f;
    static float NotificationExpireDuration = 2.0f;
    static double NotificationUpdateFrequency = 2.0f;

    // Check whether we want to display Slate cooking and instantiation notifications.
    bool bDisplaySlateCookingNotifications = false;
    if ( HoudiniRuntimeSettings )
        bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

    if ( HapiGUID.IsValid() )
    {
        // If we have a valid task GUID.
        if ( FHoudiniEngine::Get().RetrieveTaskInfo( HapiGUID, TaskInfo ) )
        {
            if ( EHoudiniEngineTaskState::None != TaskInfo.TaskState )
            {
                if ( !NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
                {
                    FNotificationInfo Info( TaskInfo.StatusText );

                    Info.bFireAndForget = false;
                    Info.FadeOutDuration = NotificationFadeOutDuration;
                    Info.ExpireDuration = NotificationExpireDuration;

                    TSharedPtr< FSlateDynamicImageBrush > HoudiniBrush = FHoudiniEngine::Get().GetHoudiniLogoBrush();
                    if ( HoudiniBrush.IsValid() )
                        Info.Image = HoudiniBrush.Get();

                    if ( ( FPlatformTime::Seconds() - HapiNotificationStarted) >= NotificationUpdateFrequency )
                    {
                        if ( !IsPIEActive() )
                            NotificationPtr = FSlateNotificationManager::Get().AddNotification( Info );
                    }
                }
            }

            FString DisplayName;
            if (GetOwner())
                DisplayName = (GetOwner()->GetName());
            else
                DisplayName = GetName();

            switch( TaskInfo.TaskState )
            {
                case EHoudiniEngineTaskState::FinishedInstantiation:
                {
                    HOUDINI_LOG_MESSAGE( TEXT("    %s FinishedInstantiation." ), *DisplayName );

                    if ( FHoudiniEngineUtils::IsValidNodeId( TaskInfo.AssetId ) )
                    {
                        // Set new asset id.
                        SetAssetId( TaskInfo.AssetId );

                        AHoudiniAssetActor * HoudiniAssetActor = GetHoudiniAssetActorOwner();
                        if( HoudiniAssetActor && HoudiniAssetActor->GetName().StartsWith( AHoudiniAssetActor::StaticClass()->GetName() ) )
                        {
                            // Assign unique actor label based on asset name if it seems to have not been renamed already
                            AssignUniqueActorLabel();
                        }

                        // Create default preset buffer.
                        CreateDefaultPreset();

                        // If necessary, set asset transform.
                        if ( bUploadTransformsToHoudiniEngine )
                        {
                            // Retrieve the current component-to-world transform for this component.
                            if ( !FHoudiniEngineUtils::HapiSetAssetTransform( AssetId, GetComponentTransform() ) )
                                HOUDINI_LOG_MESSAGE( TEXT( "Failed Uploading Initial Transformation back to HAPI." ) );
                        }

                        if ( NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
                        {
                            TSharedPtr< SNotificationItem > NotificationItem = NotificationPtr.Pin();
                            if ( NotificationItem.IsValid() )
                            {
                                NotificationItem->SetText( TaskInfo.StatusText );
                                NotificationItem->ExpireAndFadeout();

                                NotificationPtr.Reset();
                            }
                        }

                        FHoudiniEngine::Get().RemoveTaskInfo( HapiGUID );
                        HapiGUID.Invalidate();

                        // We just finished instantiation, we need to reset cook counter.
                        AssetCookCount = 0;

                        if ( TaskInfo.bLoadedComponent )
                            bFinishedLoadedInstantiation = true;

                        FHoudiniEngine::Get().SetHapiState( HAPI_RESULT_SUCCESS );
                    }
                    else
                    {
                        bStopTicking = true;
                        HOUDINI_LOG_MESSAGE( TEXT( "    %s Received invalid asset id." ), *DisplayName );
                    }

                    break;
                }

                case EHoudiniEngineTaskState::FinishedCooking:
                {
                    HOUDINI_LOG_MESSAGE( TEXT( "   %s FinishedCooking." ), *DisplayName );

                    if ( FHoudiniEngineUtils::IsValidNodeId( TaskInfo.AssetId ) )
                    {
                        // Set new asset id.
                        SetAssetId( TaskInfo.AssetId );

                        // Call post cook event.
                        PostCook();

                        // Need to update rendering information.
                        UpdateRenderingInformation();

#if WITH_EDITOR
                        // Force editor to redraw viewports.
                        if ( GEditor )
                            GEditor->RedrawAllViewports();

                        // Update properties panel after instantiation.
                        UpdateEditorProperties( true );

                        // We may have toolshelf input presets to apply (may cause a recook)
                        if ( HoudiniToolInputPreset.Num() > 0 )
                            ApplyHoudiniToolInputPreset();
#endif
                    }
                    else
                    {
                        HOUDINI_LOG_MESSAGE( TEXT( "    %s Received invalid asset id." ), *DisplayName );
                    }

                    if ( NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
                    {
                        TSharedPtr< SNotificationItem > NotificationItem = NotificationPtr.Pin();
                        if ( NotificationItem.IsValid() )
                        {
                            NotificationItem->SetText( TaskInfo.StatusText );
                            NotificationItem->ExpireAndFadeout();

                            NotificationPtr.Reset();
                        }
                    }

                    FHoudiniEngine::Get().RemoveTaskInfo( HapiGUID );
                    HapiGUID.Invalidate();

                    bStopTicking = true;
                    AssetCookCount++;

                    break;
                }

                case EHoudiniEngineTaskState::FinishedCookingWithErrors:
                {
                    HOUDINI_LOG_MESSAGE( TEXT( "    %s FinishedCookingWithErrors." ), *DisplayName );

                    if ( FHoudiniEngineUtils::IsValidNodeId( TaskInfo.AssetId ) )
                    {
                        // Call post cook event with error parameter. This will create parameters, inputs and handles.
                        PostCook( true );

                        // Create default preset buffer.
                        CreateDefaultPreset();
#if WITH_EDITOR
                        // Apply the Input presets for Houdini tools if we have any...
                        ApplyHoudiniToolInputPreset();

                        // Update properties panel.
                        UpdateEditorProperties( true );
#endif
                        // If necessary, set asset transform.
                        if ( bUploadTransformsToHoudiniEngine )
                        {
                            // Retrieve the current component-to-world transform for this component.
                            if ( !FHoudiniEngineUtils::HapiSetAssetTransform(AssetId, GetComponentTransform() ) )
                                HOUDINI_LOG_MESSAGE( TEXT( "Failed Uploading Initial Transformation back to HAPI." ) );
                        }
                    }
                    else
                    {
                        // Make sure to reset the manual cook flag here to avoid endless cooking/error spam
                        bManualRecookRequested = false;
                    }

                    // Try to detect possible crash/failures of HARS
                    if ( TaskInfo.Result == HAPI_RESULT_NOT_INITIALIZED
                        || TaskInfo.Result == HAPI_RESULT_INVALID_SESSION )
                    {
                        HOUDINI_LOG_ERROR( TEXT("Failed to cook due to an invalid session.\n\
                            This could be due to a crash in the Houdini Engine session / HARS.\n\
                            Try restarting it via \"File > Restart Houdini Engine session\"\n\
                            then rebuild your assets." ) );
                    }

                    if ( NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
                    {
                        TSharedPtr< SNotificationItem > NotificationItem = NotificationPtr.Pin();
                        if ( NotificationItem.IsValid() )
                        {
                            NotificationItem->SetText( TaskInfo.StatusText );
                            NotificationItem->ExpireAndFadeout();

                            NotificationPtr.Reset();
                        }
                    }

                    FHoudiniEngine::Get().RemoveTaskInfo( HapiGUID );
                    HapiGUID.Invalidate();

                    bStopTicking = true;
                    AssetCookCount++;

                    break;
                }

                case EHoudiniEngineTaskState::Aborted:
                case EHoudiniEngineTaskState::FinishedInstantiationWithErrors:
                {
                    HOUDINI_LOG_ERROR( TEXT( "    %s FinishedInstantiationWithErrors." ), *DisplayName );

                    bool bLicensingIssue = false;
                    switch( TaskInfo.Result )
                    {
                        case HAPI_RESULT_NO_LICENSE_FOUND:
                        {
                            FHoudiniEngine::Get().SetHapiState( HAPI_RESULT_NO_LICENSE_FOUND );

                            bLicensingIssue = true;
                            break;
                        }

                        case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND:
                        case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE:
                        case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE:
                        case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE:
                        {
                            bLicensingIssue = true;
                            break;
                        }

                        default:
                        {
                            break;
                        }
                    }

                    if ( bLicensingIssue )
                    {
                        const FString & StatusMessage = TaskInfo.StatusText.ToString() ;
                        HOUDINI_LOG_MESSAGE( TEXT( "%s" ), *StatusMessage );

                        FString WarningTitle = TEXT( "Houdini Engine Plugin Warning" );
                        FText WarningTitleText = FText::FromString( WarningTitle );
                        FString WarningMessage = FString::Printf( TEXT( "Houdini License issue - %s." ), *StatusMessage );

                        FMessageDialog::Debugf( FText::FromString( WarningMessage ), &WarningTitleText );
                    }

                    if ( NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
                    {
                        TSharedPtr< SNotificationItem > NotificationItem = NotificationPtr.Pin();
                        if ( NotificationItem.IsValid() )
                        {
                            NotificationItem->SetText( TaskInfo.StatusText );
                            NotificationItem->ExpireAndFadeout();

                            NotificationPtr.Reset();
                        }
                    }

                    if ( TaskInfo.bLoadedComponent )
                        bFinishedLoadedInstantiation = true;

                    FHoudiniEngine::Get().RemoveTaskInfo( HapiGUID );
                    HapiGUID.Invalidate();

                    bStopTicking = true;
                    AssetCookCount = 0;

                    break;
                }

                case EHoudiniEngineTaskState::Processing:
                {

                    if ( NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
                    {
                        TSharedPtr< SNotificationItem > NotificationItem = NotificationPtr.Pin();
                        if ( NotificationItem.IsValid() )
                            NotificationItem->SetText( TaskInfo.StatusText );
                    }

                    break;
                }

                case EHoudiniEngineTaskState::None:
                default:
                {
                    break;
                }
            }
        }
        else
        {
            // Task information does not exist, we can stop ticking.
            HapiGUID.Invalidate();
            bStopTicking = true;
        }
    }

    if (bFinishedLoadedInstantiation)
        bAssetIsBeingInstantiated = false;

    if ( !IsInstantiatingOrCooking() )
    {
        if (( HasBeenInstantiatedButNotCooked() || bParametersChanged || bComponentNeedsCook || bManualRecookRequested )
            && (FHoudiniEngine::Get().GetEnableCookingGlobal() || bManualRecookRequested) ) 
        {
            // Grab current time for delayed notification.
            HapiNotificationStarted = FPlatformTime::Seconds();

            // We just submitted a task, we want to continue ticking.
            bStopTicking = false;

            if ( bWaitingForUpstreamAssetsToInstantiate )
            {
                // We are waiting for upstream assets to instantiate. Update the flag
                UpdateWaitingForUpstreamAssetsToInstantiate();

                // Try instantiating this asset again.
                if ( !bWaitingForUpstreamAssetsToInstantiate )
                    bLoadedComponentRequiresInstantiation = true;
            }
            else if ( bLoadedComponentRequiresInstantiation )
            {
                // This component has been loaded and requires instantiation.
                bLoadedComponentRequiresInstantiation = false;
                StartTaskAssetInstantiation( true );
            }
            else if ( bFinishedLoadedInstantiation )
            {
                // If we are doing first cook after instantiation.
                RefreshEditableNodesAfterLoad();

                // Update parameter node id for all loaded parameters.
                UpdateLoadedParameters();

                // Additionally, we need to update and create assets for all input parameters that have geos assigned.
                UpdateLoadedInputs( bManualRecookRequested );

                // We also need to upload loaded curve points.
                UploadLoadedCurves();

                // If we finished loading instantiation, we can restore preset data.
                if ( PresetBuffer.Num() > 0 )
                {
                    FHoudiniEngineUtils::SetAssetPreset( AssetId, PresetBuffer );
                    PresetBuffer.Empty();
                }

                // Upload changed parameters back to HAPI.
                UploadChangedParameters();

                // Reset tranform changed flag.
                bComponentNeedsCook = false;

                // Create asset cooking task object and submit it for processing.
                StartTaskAssetCooking();
            }
            else
            {
                if ( IsCookingEnabled() || bManualRecookRequested )
                {
                    // Upload changed parameters back to HAPI.
                    UploadChangedParameters();

                    // Create asset cooking task object and submit it for processing.
                    StartTaskAssetCooking();

                    // Reset ComponentNeedsCook flag.
                    bComponentNeedsCook = false;
                }
                else
                {
                    // Cooking is disabled, but we still need to upload the parameters
                    // and update the editor properties
                    UploadChangedParameters();

                    // Update properties panel.
                    UpdateEditorProperties(true);

                    // Remember that we have uncooked changes
                    bComponentNeedsCook = true;

                    // Stop ticking
                    bStopTicking = true;
                }
            }
        }
        else
        {
            // Nothing has changed, we can terminate ticking.
            bStopTicking = true;
        }
    }

    if ( bNeedToUpdateNavigationSystem )
    {
#ifdef WITH_EDITOR
        // notify navigation system
        AHoudiniAssetActor* HoudiniActor = GetHoudiniAssetActorOwner();
        if ( HoudiniActor && !HoudiniActor->IsPendingKill() )
            FNavigationSystem::UpdateActorAndComponentData(*HoudiniActor);
#endif
        bNeedToUpdateNavigationSystem = false;
    }

    if ( bStopTicking )
        StopHoudiniTicking();
}

void
UHoudiniAssetComponent::UpdateEditorProperties( bool bConditionalUpdate )
{
    AHoudiniAssetActor * HoudiniAssetActor = GetHoudiniAssetActorOwner();
    if ( !HoudiniAssetActor )
        return;

    FPropertyEditorModule & PropertyModule =
        FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

    // We want to iterate on all the details panel
    static const FName DetailsTabIdentifiers[] = { "LevelEditorSelectionDetails", "LevelEditorSelectionDetails2", "LevelEditorSelectionDetails3", "LevelEditorSelectionDetails4" };
    for (const FName& DetailsPanelName : DetailsTabIdentifiers )
    {
        // Locate the details panel.
        //FName DetailsPanelName = "LevelEditorSelectionDetails";
        TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

        if ( !DetailsView.IsValid() )
        {
            // We have no details panel, nothing to update.
            continue;
        }

        if ( DetailsView->IsLocked() )
        {
            // If details panel is locked, locate selected actors and check if this component belongs to one of them.
            const TArray< TWeakObjectPtr< AActor > > & SelectedDetailActors = DetailsView->GetSelectedActors();
            bool bFoundActor = false;

            for (int32 ActorIdx = 0, ActorNum = SelectedDetailActors.Num(); ActorIdx < ActorNum; ++ActorIdx)
            {
                TWeakObjectPtr< AActor > SelectedActor = SelectedDetailActors[ActorIdx];
                if ( SelectedActor.IsValid() && SelectedActor.Get() == HoudiniAssetActor )
                {
                    bFoundActor = true;
                    break;
                }
            }

            // Details panel is locked, but our actor is not selected.
            if ( !bFoundActor )
                continue;
        }
        else
        {
            // If our actor is not selected (and details panel is not locked) don't do any updates.
            if ( !HoudiniAssetActor->IsSelected() )
                continue;
        }

        if ( GEditor && HoudiniAssetActor && bIsNativeComponent )
        {
            if ( bConditionalUpdate && FSlateApplication::Get().HasAnyMouseCaptor() )
            {
                // We want to avoid UI update if this is a conditional update and widget has captured the mouse.
                StartHoudiniUIUpdateTicking();
                continue;
            }

            TArray< UObject * > SelectedActors;
            SelectedActors.Add( HoudiniAssetActor );

            // bEditorPropertiesNeedFullUpdate is false only when small changes (parameters value) have been made
            // We do not reselect the actor to avoid loosing the current selected parameter

            // Reset selected actor to itself, force refresh and override the lock.
            DetailsView->SetObjects( SelectedActors, bEditorPropertiesNeedFullUpdate, true );

            if ( !bEditorPropertiesNeedFullUpdate )
                bEditorPropertiesNeedFullUpdate = true;

            if ( GUnrealEd )
            {
                GUnrealEd->UpdateFloatingPropertyWindows();
            }
        }

        StopHoudiniUIUpdateTicking();
    }
}

void
UHoudiniAssetComponent::StartTaskAssetInstantiation( bool bLocalLoadedComponent, bool bStartTicking )
{
    // We do not want to be instantiated twice
    bAssetIsBeingInstantiated = true;

    // We first need to make sure all our asset inputs have been instantiated and reconnected.
    UpdateWaitingForUpstreamAssetsToInstantiate( true );

    if ( !bWaitingForUpstreamAssetsToInstantiate )
    {
        // Check if asset has multiple Houdini assets inside.
        HAPI_AssetLibraryId AssetLibraryId = -1;
        TArray< HAPI_StringHandle > AssetNames;

        if ( FHoudiniEngineUtils::GetAssetNames( HoudiniAsset, AssetLibraryId, AssetNames ) )
        {
            HAPI_StringHandle PickedAssetName = AssetNames[ 0 ];
            bool bShowMultiAssetDialog = false;

            const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
            if ( HoudiniRuntimeSettings )
                bShowMultiAssetDialog = HoudiniRuntimeSettings->bShowMultiAssetDialog;

            if ( bShowMultiAssetDialog && AssetNames.Num() > 1 )
            {
                // If we have more than one asset, we need to present user with choice dialog.

                TSharedPtr< SWindow > ParentWindow;

                // Check if the main frame is loaded. When using the old main frame it may not be.
                if ( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
                {
                    IMainFrameModule & MainFrame = FModuleManager::LoadModuleChecked< IMainFrameModule >( "MainFrame" );
                    ParentWindow = MainFrame.GetParentWindow();
                }

                if ( ParentWindow.IsValid() )
                {
                    TSharedPtr< SAssetSelectionWidget > AssetSelectionWidget;

                    TSharedRef< SWindow > Window = SNew( SWindow )
                        .Title( LOCTEXT( "WindowTitle", "Select an asset to instantiate" ) )
                        .ClientSize( FVector2D( 640, 480 ) )
                        .SupportsMinimize( false )
                        .SupportsMaximize( false )
                        .HasCloseButton( false );

                    Window->SetContent( SAssignNew( AssetSelectionWidget, SAssetSelectionWidget )
                        .WidgetWindow( Window )
                        .AvailableAssetNames( AssetNames ) );

                    if ( AssetSelectionWidget->IsValidWidget() )
                    {
                        FSlateApplication::Get().AddModalWindow( Window, ParentWindow, false );

                        int32 DialogPickedAssetName = AssetSelectionWidget->GetSelectedAssetName();
                        if ( DialogPickedAssetName != -1 )
                            PickedAssetName = DialogPickedAssetName;
                    }
                }
            }

            // Create new GUID to identify this request.
            HapiGUID = FGuid::NewGuid();

            FHoudiniEngineTask Task( EHoudiniEngineTaskType::AssetInstantiation, HapiGUID );
            Task.Asset = HoudiniAsset;
            Task.ActorName = GetOuter()->GetName();
            Task.bLoadedComponent = bLocalLoadedComponent;
            Task.AssetLibraryId = AssetLibraryId;
            Task.AssetHapiName = PickedAssetName;
            FHoudiniEngine::Get().AddTask( Task );
        }
        else
        {
            HOUDINI_LOG_MESSAGE( TEXT( "Cancelling asset instantiation - unable to retrieve asset names." ) );
            return;
        }
    }

    // Start ticking - this will poll the cooking system for completion.
    if ( bStartTicking )
        StartHoudiniTicking();
}

void
UHoudiniAssetComponent::StartTaskAssetCookingManual()
{
    if ( !IsInstantiatingOrCooking() )
    {
        bManualRecookRequested = true;
        if ( FHoudiniEngineUtils::IsValidNodeId( GetAssetId() ) )
        {
            StartHoudiniTicking();
        }
        else
        {
            if ( bLoadedComponent )
            {
                // This is a loaded component which requires instantiation.
                StartTaskAssetInstantiation( true, true );
                bParametersChanged = true;
            }
        }
    }
}

void
UHoudiniAssetComponent::StartTaskAssetResetManual()
{
    if ( !IsInstantiatingOrCooking() )
    {
        if ( FHoudiniEngineUtils::IsValidNodeId( GetAssetId() ) )
        {
            if ( FHoudiniEngineUtils::SetAssetPreset( GetAssetId(), DefaultPresetBuffer ) )
            {
                UnmarkChangedParameters();
                StartTaskAssetCookingManual();
            }
        }
        else
        {
            if ( bLoadedComponent )
            {
                // This is a loaded component which requires instantiation.
                bLoadedComponentRequiresInstantiation = true;
                bParametersChanged = true;

                // Replace serialized preset buffer with default preset buffer.
                PresetBuffer = DefaultPresetBuffer;
                StartHoudiniTicking();
            }
        }
    }
}

void
UHoudiniAssetComponent::StartTaskAssetRebuildManual()
{
    if ( !IsInstantiatingOrCooking() )
    {
        if ( FHoudiniEngineUtils::IsHoudiniNodeValid( AssetId ) )
        {
            if ( !FHoudiniEngineUtils::GetAssetPreset( AssetId, PresetBuffer ) )
            {
                HOUDINI_LOG_WARNING(TEXT("Failed to get the asset's preset, rebuilt asset may have lost its parameters."));
            }

            // We need to delete the asset and request a new one.
            StartTaskAssetDeletion();
        }

        HapiGUID = FGuid::NewGuid();

        // If this is a loaded component, then we just need to instantiate.
        bLoadedComponentRequiresInstantiation = true;
        bParametersChanged = true;
        bManualRecookRequested = true;

        StartHoudiniTicking();
    }
}

void
UHoudiniAssetComponent::StartTaskAssetDeletion()
{
    if ( FHoudiniEngineUtils::IsValidNodeId( AssetId ) && bIsNativeComponent )
    {
        // Get the Asset's NodeInfo
        HAPI_NodeInfo AssetNodeInfo;
        FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
        FHoudiniApi::GetNodeInfo(
            FHoudiniEngine::Get().GetSession(), AssetId, &AssetNodeInfo );

        HAPI_NodeId OBJNodeToDelete = AssetId;
        if ( AssetNodeInfo.type == HAPI_NODETYPE_SOP )
        {
            // For SOP Asset, we want to delete their parent's OBJ node
            HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId( AssetId );
            OBJNodeToDelete = ParentId != -1 ? ParentId : AssetId;
        }

        // Generate GUID for our new task.
        FGuid HapiDeletionGUID = FGuid::NewGuid();

        // Create asset deletion task object and submit it for processing.
        FHoudiniEngineTask Task( EHoudiniEngineTaskType::AssetDeletion, HapiDeletionGUID );
        Task.AssetId = OBJNodeToDelete;
        FHoudiniEngine::Get().AddTask( Task );

        // Reset asset id
        AssetId = -1;

        // We do not need to tick as we are not interested in result.
    }
}

void
UHoudiniAssetComponent::StartTaskAssetCooking( bool bStartTicking )
{
    if ( !IsInstantiatingOrCooking() )
    {
        if (!FHoudiniEngineUtils::IsValidNodeId(AssetId))
            return;

        // Generate GUID for our new task.
        HapiGUID = FGuid::NewGuid();

        FHoudiniEngineTask Task( EHoudiniEngineTaskType::AssetCooking, HapiGUID );
        Task.ActorName = GetOuter()->GetName();
        Task.AssetId = GetAssetId();
        FHoudiniEngine::Get().AddTask( Task );

        if ( bStartTicking )
            StartHoudiniTicking();
    }
}

void
UHoudiniAssetComponent::ResetHoudiniResources()
{
    if ( HapiGUID.IsValid() )
    {
        // If we have a valid task GUID.
        FHoudiniEngineTaskInfo TaskInfo;

        if ( FHoudiniEngine::Get().RetrieveTaskInfo( HapiGUID, TaskInfo ) )
        {
            FHoudiniEngine::Get().RemoveTaskInfo( HapiGUID );
            HapiGUID.Invalidate();
            StopHoudiniTicking();

            // Get settings.
            const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

            // Check whether we want to display Slate cooking and instantiation notifications.
            bool bDisplaySlateCookingNotifications = false;
            if ( HoudiniRuntimeSettings )
                bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

            if ( NotificationPtr.IsValid() && bDisplaySlateCookingNotifications )
            {
                TSharedPtr< SNotificationItem > NotificationItem = NotificationPtr.Pin();
                if ( NotificationItem.IsValid() )
                {
                    NotificationItem->ExpireAndFadeout();
                    NotificationPtr.Reset();
                }
            }
        }
    }

    // Start asset deletion.
    StartTaskAssetDeletion();
}

void
UHoudiniAssetComponent::SubscribeEditorDelegates()
{
    // Add delegate for asset post import.
    DelegateHandleAssetPostImport =
        FEditorDelegates::OnAssetPostImport.AddUObject( this, &UHoudiniAssetComponent::OnAssetPostImport );

    // Add delegate for viewport drag and drop events.
    DelegateHandleApplyObjectToActor =
        FEditorDelegates::OnApplyObjectToActor.AddUObject( this, &UHoudiniAssetComponent::OnApplyObjectToActor );

    if ( GEditor )
    {
        GEditor->OnActorMoved().AddUObject( this, &UHoudiniAssetComponent::OnActorMoved );
    }
}

void
UHoudiniAssetComponent::UnsubscribeEditorDelegates()
{
    // Remove delegate for asset post import.
    FEditorDelegates::OnAssetPostImport.Remove( DelegateHandleAssetPostImport );

    // Remove delegate for viewport drag and drop events.
    FEditorDelegates::OnApplyObjectToActor.Remove( DelegateHandleApplyObjectToActor );

    if ( GEditor )
    {
        GEditor->OnActorMoved().RemoveAll( this );
    }
}

void
UHoudiniAssetComponent::PostEditChangeProperty( FPropertyChangedEvent & PropertyChangedEvent )
{
    Super::PostEditChangeProperty( PropertyChangedEvent );

    if (!bIsNativeComponent)
        return;

    UProperty * Property = PropertyChangedEvent.MemberProperty;
    if ( !Property )
        return;

    if ( Property->GetName() == TEXT( "Mobility" ) )
    {
        // Changed GetAttachChildren to 'GetAllDescendants' due to HoudiniMeshSplitInstanceComponent 
        // not propagating property changes to their own child StaticMeshComponents.
        TArray< USceneComponent * > LocalAttachChildren;
        GetChildrenComponents(true, LocalAttachChildren);

        // Mobility was changed, we need to update it for all attached components as well.
        for ( TArray< USceneComponent * >::TConstIterator Iter( LocalAttachChildren ); Iter; ++Iter)
        {
            USceneComponent * SceneComponent = *Iter;
            SceneComponent->SetMobility( Mobility );
        }

        return;
    }
    else if ( Property->GetName() == TEXT( "bVisible" ) )
    {
        // Visibility has changed, propagate it to children.
        SetVisibility( bVisible, true );
        return;
    }
    else if ( ( Property->GetName() == TEXT( "RelativeLocation" ) )
            || (Property->GetName() == TEXT( "RelativeRotation" ) )
            || (Property->GetName() == TEXT( "RelativeScale3D" ) ) )
    {
        // Location has changed
        CheckedUploadTransform();
    }
    else if ( Property->GetName() == TEXT( "bHiddenInGame" ) )
    {
        // Visibility has changed, propagate it to children.
        SetHiddenInGame( bHiddenInGame, true );
        return;
    }

    if ( Property->HasMetaData( TEXT( "Category" ) ) )
    {
        const FString & Category = Property->GetMetaData( TEXT( "Category" ) );
        static const FString CategoryHoudiniGeneratedStaticMeshSettings = TEXT( "HoudiniGeneratedStaticMeshSettings" );
        static const FString CategoryLighting = TEXT( "Lighting" );
        static const FString CategoryRendering = TEXT( "Rendering" );
        static const FString CategoryCollision = TEXT( "Collision" );
        static const FString CategoryPhysics = TEXT("Physics");
        static const FString CategoryLOD = TEXT("LOD");

        if ( CategoryHoudiniGeneratedStaticMeshSettings == Category )
        {
            // We are changing one of the mesh generation properties, we need to update all static meshes.
            // As the StaticMeshComponents map contains only top-level static mesh components only, use the StaticMeshes map instead
            for (TMap< FHoudiniGeoPartObject, UStaticMesh * > ::TIterator Iter(StaticMeshes); Iter; ++Iter)
            {
                UStaticMesh * StaticMesh = Iter.Value();
                if ( !StaticMesh || StaticMesh->IsPendingKill() )
                    continue;

                SetStaticMeshGenerationParameters( StaticMesh );
                FHoudiniScopedGlobalSilence ScopedGlobalSilence;
                StaticMesh->Build( true );
                RefreshCollisionChange( *StaticMesh );
            }

            return;
        }
        else if ( CategoryLighting == Category )
        {
            if ( Property->GetName() == TEXT( "CastShadow" ) )
            {
                // Stop cast-shadow being applied to invisible colliders children
                // This prevent colliders only meshes from casting shadows
                TArray< UActorComponent * > ReregisterComponents;
                {
                    TArray< USceneComponent * > LocalAttachChildren;
                    GetChildrenComponents(true, LocalAttachChildren);
                    for (TArray< USceneComponent * >::TConstIterator Iter(LocalAttachChildren); Iter; ++Iter)
                    {
                        UStaticMeshComponent * Component = Cast< UStaticMeshComponent >(*Iter);
                        if (!Component || Component->IsPendingKill())
                            continue;

                        const FHoudiniGeoPartObject * pGeoPart = StaticMeshes.FindKey(Component->GetStaticMesh());
                        if (pGeoPart && pGeoPart->IsCollidable())
                        {
                            // This is an invisible collision mesh:
                            // Do not interfere with lightmap builds - disable shadow casting
                            Component->SetCastShadow(false);
                        }
                        else
                        {
                            // Set normally
                            Component->SetCastShadow(CastShadow);
                        }

                        ReregisterComponents.Add(Component);
                    }
                }

                if (ReregisterComponents.Num() > 0)
                {
                    FMultiComponentReregisterContext MultiComponentReregisterContext(ReregisterComponents);
                }
            }
            else if ( Property->GetName() == TEXT( "bCastDynamicShadow" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCastDynamicShadow );
            }
            else if ( Property->GetName() == TEXT( "bCastStaticShadow" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCastStaticShadow );
            }
            else if ( Property->GetName() == TEXT( "bCastVolumetricTranslucentShadow" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCastVolumetricTranslucentShadow );
            }
            else if ( Property->GetName() == TEXT( "bCastInsetShadow" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCastInsetShadow );
            }
            else if ( Property->GetName() == TEXT( "bCastHiddenShadow" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCastHiddenShadow );
            }
            else if ( Property->GetName() == TEXT( "bCastShadowAsTwoSided" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCastShadowAsTwoSided );
            }
            /*else if ( Property->GetName() == TEXT( "bLightAsIfStatic" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bLightAsIfStatic );
            }*/
            else if ( Property->GetName() == TEXT( "bLightAttachmentsAsGroup" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bLightAttachmentsAsGroup );
            }
            else if ( Property->GetName() == TEXT( "IndirectLightingCacheQuality" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, IndirectLightingCacheQuality );
            }
        }
        else if ( CategoryRendering == Category )
        {
            if ( Property->GetName() == TEXT( "bVisibleInReflectionCaptures" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bVisibleInReflectionCaptures );
            }
            else if ( Property->GetName() == TEXT( "bRenderInMainPass" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bRenderInMainPass );
            }
            else if ( Property->GetName() == TEXT( "bRenderInMono" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bRenderInMono );
            }
            else if ( Property->GetName() == TEXT( "bOwnerNoSee" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bOwnerNoSee );
            }
            else if ( Property->GetName() == TEXT( "bOnlyOwnerSee" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bOnlyOwnerSee );
            }
            else if ( Property->GetName() == TEXT( "bTreatAsBackgroundForOcclusion" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bTreatAsBackgroundForOcclusion );
            }
            else if ( Property->GetName() == TEXT( "bUseAsOccluder" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bUseAsOccluder );
            }
            else if ( Property->GetName() == TEXT( "bRenderCustomDepth" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bRenderCustomDepth );
            }
            else if ( Property->GetName() == TEXT( "CustomDepthStencilValue" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, CustomDepthStencilValue );
            }
            else if ( Property->GetName() == TEXT( "CustomDepthStencilWriteMask" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, CustomDepthStencilWriteMask );
            }
            else if ( Property->GetName() == TEXT( "TranslucencySortPriority" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, TranslucencySortPriority );
            }
            else if ( Property->GetName() == TEXT( "LpvBiasMultiplier" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, LpvBiasMultiplier );
            }
            else if ( Property->GetName() == TEXT( "bReceivesDecals" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bReceivesDecals );
            }
            else if ( Property->GetName() == TEXT( "BoundsScale" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, BoundsScale );
            }
            else if ( Property->GetName() == TEXT( "bUseAttachParentBound" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( USceneComponent, bUseAttachParentBound);
            }
        }
        else if ( CategoryCollision == Category )
        {
            if ( Property->GetName() == TEXT( "bAlwaysCreatePhysicsState" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bAlwaysCreatePhysicsState );
            }
            /*else if ( Property->GetName() == TEXT( "bGenerateOverlapEvents" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bGenerateOverlapEvents );
            }*/
            else if ( Property->GetName() == TEXT( "bMultiBodyOverlap" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bMultiBodyOverlap );
            }
            else if ( Property->GetName() == TEXT( "bCheckAsyncSceneOnMove" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bCheckAsyncSceneOnMove );
            }
            else if ( Property->GetName() == TEXT( "bTraceComplexOnMove" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bTraceComplexOnMove );
            }
            else if ( Property->GetName() == TEXT( "bReturnMaterialOnMove" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bReturnMaterialOnMove );
            }
            else if ( Property->GetName() == TEXT( "BodyInstance" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, BodyInstance );
            }
            else if ( Property->GetName() == TEXT( "CanCharacterStepUpOn" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, CanCharacterStepUpOn );
            }
            /*else if ( Property->GetName() == TEXT( "bCanEverAffectNavigation" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UActorComponent, bCanEverAffectNavigation );
            }*/
        }
        else if ( CategoryPhysics == Category )
        {
            if ( Property->GetName() == TEXT( "bIgnoreRadialImpulse" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bIgnoreRadialImpulse );
            }
            else if ( Property->GetName() == TEXT( "bIgnoreRadialForce" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bIgnoreRadialForce );
            }
            else if ( Property->GetName() == TEXT( "bApplyImpulseOnDamage" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bApplyImpulseOnDamage );
            }
            /*
            else if ( Property->GetName() == TEXT( "bShouldUpdatePhysicsVolume" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( USceneComponent, bShouldUpdatePhysicsVolume );
            }
            */
        }
        else if ( CategoryLOD == Category )
        {
            if ( Property->GetName() == TEXT( "MinDrawDistance" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, MinDrawDistance );
            }
            else if ( Property->GetName() == TEXT( "LDMaxDrawDistance" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, LDMaxDrawDistance );
            }
            else if ( Property->GetName() == TEXT( "CachedMaxDrawDistance" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, CachedMaxDrawDistance );
            }
            else if ( Property->GetName() == TEXT( "bAllowCullDistanceVolume" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bAllowCullDistanceVolume );
            }
            else if ( Property->GetName() == TEXT( "DetailMode" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( USceneComponent, DetailMode );
            }
        }
    }
}

void
UHoudiniAssetComponent::RemoveAllAttachedComponents()
{
    while ( true )
    {
        const int32 ChildCount = GetAttachChildren().Num();
        if ( ChildCount <= 0 )
            break;

        USceneComponent * Component = GetAttachChildren()[ ChildCount - 1 ];

        Component->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
        Component->UnregisterComponent();
        Component->DestroyComponent();
    }

    check( GetAttachChildren().Num() == 0 );
}

void
UHoudiniAssetComponent::OnComponentClipboardCopy( UHoudiniAssetComponent * HoudiniAssetComponent )
{
    // Store copied component.
    CopiedHoudiniComponent = HoudiniAssetComponent;

    if ( bIsNativeComponent )
    {
        // This component has been loaded.
        bLoadedComponent = true;
        bFullyLoaded = false;
    }

    // Mark this component as imported.
    bComponentCopyImported = true;
}

void
UHoudiniAssetComponent::OnAssetPostImport( UFactory * Factory, UObject * Object )
{
    if (!bComponentCopyImported || !CopiedHoudiniComponent.IsValid() )
        return;

    // Show busy cursor.
    FScopedBusyCursor ScopedBusyCursor;

    // Copy the original scale - this gets lost sometimes in the copy/paste procedure
    SetWorldScale3D( CopiedHoudiniComponent->GetComponentScale() );

    // Copy mobility
    SetMobility( CopiedHoudiniComponent->Mobility );

    // Get original asset id.
    HAPI_NodeId CopiedHoudiniComponentAssetId = CopiedHoudiniComponent->AssetId;

    // Set Houdini asset.
    HoudiniAsset = CopiedHoudiniComponent->HoudiniAsset;

    // Copy preset buffer.
    if ( FHoudiniEngineUtils::IsValidNodeId( CopiedHoudiniComponentAssetId ) )
        FHoudiniEngineUtils::GetAssetPreset( CopiedHoudiniComponentAssetId, PresetBuffer );
    else
        PresetBuffer = CopiedHoudiniComponent->PresetBuffer;

    // Copy default preset buffer.
    DefaultPresetBuffer = CopiedHoudiniComponent->DefaultPresetBuffer;

    // Clean up all generated and auto-attached components.
    RemoveAllAttachedComponents();

    // Release static mesh related resources.
    ReleaseObjectGeoPartResources( StaticMeshes );
    StaticMeshes.Empty();
    StaticMeshComponents.Empty();

    TMap<UObject*, UObject*> ReplacementMap;

    // We need to reconstruct geometry from copied actor.
    for( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( CopiedHoudiniComponent->StaticMeshes ); Iter; ++Iter )
    {
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        UStaticMesh * StaticMesh = Iter.Value();
        if ( !StaticMesh || StaticMesh->IsPendingKill() )
            continue;

        // Prevents the default Houdini logo mesh from being duplicated and saved into sub-levels
        const UStaticMesh * HoudiniLogoMesh = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
        if (StaticMesh == HoudiniLogoMesh)
        {
            HOUDINI_LOG_WARNING(TEXT("Ignoring HoudiniAsset duplication of default logo static mesh."));
            continue;
        }

        // Duplicate static mesh and all related generated Houdini materials and textures.
        UStaticMesh * DuplicatedStaticMesh =
            FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage( StaticMesh, this, HoudiniGeoPartObject, FHoudiniCookParams::GetDefaultStaticMeshesCookMode() );

        if( DuplicatedStaticMesh && !DuplicatedStaticMesh->IsPendingKill() )
        {
            // Store this duplicated mesh.
            StaticMeshes.Add( FHoudiniGeoPartObject( HoudiniGeoPartObject, true ), DuplicatedStaticMesh );
            ReplacementMap.Add( StaticMesh, DuplicatedStaticMesh );
        }
    }

    // Copy material information.
    HoudiniAssetComponentMaterials = CopiedHoudiniComponent->HoudiniAssetComponentMaterials->Duplicate( this, ReplacementMap );

    // Copy parameters.
    {
        ClearParameters();
        CopiedHoudiniComponent->DuplicateParameters( this );
    }

    // Copy inputs.
    {
        ClearInputs();
        CopiedHoudiniComponent->DuplicateInputs( this );
    }

    // Copy instance inputs.
    {
        ClearInstanceInputs();
        CopiedHoudiniComponent->DuplicateInstanceInputs( this, ReplacementMap );
    }

    // We need to reconstruct splines.
    for( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* >::TIterator
        Iter( CopiedHoudiniComponent->SplineComponents ); Iter; ++Iter )
    {
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
        if ( !HoudiniSplineComponent || !HoudiniSplineComponent->IsValidLowLevel() )
            continue;
            
        // Duplicate spline component.
        UHoudiniSplineComponent * DuplicatedSplineComponent = 
            DuplicateObject< UHoudiniSplineComponent >( HoudiniSplineComponent, this );

        if ( DuplicatedSplineComponent )
        {
            DuplicatedSplineComponent->SetFlags( RF_Transactional | RF_Public );
            SplineComponents.Add( HoudiniGeoPartObject, DuplicatedSplineComponent );
        }
    }
    
    /*
    // We need to duplicate landscapes.
    for ( TMap< FHoudiniGeoPartObject, ALandscapeProxy * >::TIterator
        Iter(CopiedHoudiniComponent->LandscapeComponents); Iter; ++Iter)
    {
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        ALandscapeProxy * HoudiniLandscape = Iter.Value();

        // Duplicate landscape component.
        ALandscapeProxy * DuplicatedLandscape =
            DuplicateObject< ALandscapeProxy >( HoudiniLandscape, this );

        if ( DuplicatedLandscape )
        {
            DuplicatedLandscape->SetFlags(RF_Transactional | RF_Public);
            LandscapeComponents.Add(HoudiniGeoPartObject, DuplicatedLandscape);
        }
    }
    */

    // We cannot duplicate landscape for now...
    // we will have to recook the asset to recreate them
    bool bNeedsRecook = false;
    if ( CopiedHoudiniComponent->LandscapeComponents.Num() > 0 )
        bNeedsRecook = true;

    // Perform any necessary post loading.
    PostLoad();

    DuplicateHandles( CopiedHoudiniComponent.Get() );

    // Mark this component as no longer copy imported and reset copied component.
    bComponentCopyImported = false;
    CopiedHoudiniComponent = nullptr;

    if ( bNeedsRecook )
        StartTaskAssetCookingManual();
}

void
UHoudiniAssetComponent::OnApplyObjectToActor( UObject* ObjectToApply, AActor * ActorToApplyTo )
{
    if ( GetHoudiniAssetActorOwner() != ActorToApplyTo )
        return;

    // We want to handle material replacements.
    UMaterialInterface * Material = Cast< UMaterialInterface >( ObjectToApply );
    if (!Material)
        return;

    bool bMaterialReplaced = false;

    TMap< UStaticMesh*, int32 > MaterialReplacementsMap;

    // We need to detect which components have material overriden, and replace it on their corresponding
    // generated static meshes.
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMesh * StaticMesh = Iter.Key();
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();

        if ( !StaticMeshComponent || !StaticMesh || StaticMeshComponent->IsPendingKill() || StaticMesh->IsPendingKill() )
            continue;

        const TArray< class UMaterialInterface * > & OverrideMaterials = StaticMeshComponent->OverrideMaterials;
        for ( int32 MaterialIdx = 0; MaterialIdx < OverrideMaterials.Num(); ++MaterialIdx )
        {
            UMaterialInterface * OverridenMaterial = OverrideMaterials[ MaterialIdx ];
            if ( OverridenMaterial && OverridenMaterial == Material )
            {
                if ( MaterialIdx < StaticMesh->StaticMaterials.Num() )
                    MaterialReplacementsMap.Add( StaticMesh, MaterialIdx );
            }
        }
    }

    for ( auto& InstanceInput : InstanceInputs )
    {
        if ( InstanceInput && !InstanceInput->IsPendingKill() )
            InstanceInput->GetMaterialReplacementMeshes( Material, MaterialReplacementsMap );
    }

    if (MaterialReplacementsMap.Num() <= 0)
        return;

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniMaterialReplacement", "Houdini Material Replacement" ), this );

    for ( TMap< UStaticMesh *, int32 >::TIterator Iter( MaterialReplacementsMap ); Iter; ++Iter )
    {
        UStaticMesh * StaticMesh = Iter.Key();
        int32 MaterialIdx = Iter.Value();

        // Get old material.
        UMaterialInterface * OldMaterial = StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface;

        // Locate geo part object.
        FHoudiniGeoPartObject HoudiniGeoPartObject = LocateGeoPartObject( StaticMesh );
        if ( !HoudiniGeoPartObject.IsValid() )
            continue;

        if ( ReplaceMaterial( HoudiniGeoPartObject, Material, OldMaterial, MaterialIdx ) )
        {
            StaticMesh->Modify();
            StaticMesh->StaticMaterials[ MaterialIdx ].MaterialInterface = Material;

            StaticMesh->PreEditChange( nullptr );
            StaticMesh->PostEditChange();
            StaticMesh->MarkPackageDirty();

            UStaticMeshComponent * StaticMeshComponent = LocateStaticMeshComponent( StaticMesh );
            if ( StaticMeshComponent )
            {
                StaticMeshComponent->Modify();
                StaticMeshComponent->SetMaterial( MaterialIdx, Material );

                bMaterialReplaced = true;
            }

            TArray< UInstancedStaticMeshComponent * > InstancedStaticMeshComponents;
            if ( LocateInstancedStaticMeshComponents( StaticMesh, InstancedStaticMeshComponents ) )
            {
                for ( int32 Idx = 0; Idx < InstancedStaticMeshComponents.Num(); ++Idx )
                {
                    UInstancedStaticMeshComponent * InstancedStaticMeshComponent = InstancedStaticMeshComponents[ Idx ];

                    if ( InstancedStaticMeshComponent )
                    {
                        InstancedStaticMeshComponent->Modify();
                        InstancedStaticMeshComponent->SetMaterial( MaterialIdx, Material );

                        bMaterialReplaced = true;
                    }
                }
            }
        }
    }

    if ( bMaterialReplaced )
        UpdateEditorProperties( false );
}

void
UHoudiniAssetComponent::OnActorMoved( AActor* Actor )
{
    if ( GetOwner() == Actor )
    {
        CheckedUploadTransform();
    }
}

void
UHoudiniAssetComponent::CreateDefaultPreset()
{
    if ( !bLoadedComponent && !FHoudiniEngineUtils::GetAssetPreset( GetAssetId(), DefaultPresetBuffer ) )
        DefaultPresetBuffer.Empty();
}

void
UHoudiniAssetComponent::OnUpdateTransform( EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport )
{
    Super::OnUpdateTransform( UpdateTransformFlags, Teleport );

    CheckedUploadTransform();
}

void
UHoudiniAssetComponent::CheckedUploadTransform()
{
    // Only if the asset is done loading, else this might cause a cook 
    // upon loading a map if the asset has TransformChangeTRiggersCook enabled
    if ( !bFullyLoaded )
        return;

    // If we have to upload transforms.
    if ( bUploadTransformsToHoudiniEngine && AssetCookCount > 0 )
    {
        // Retrieve the current component-to-world transform for this component.
        if ( !FHoudiniEngineUtils::HapiSetAssetTransform( AssetId, GetComponentTransform() ) )
            HOUDINI_LOG_MESSAGE( TEXT( "Failed Uploading Transformation change back to HAPI." ) );
    }

    // If transforms trigger cooks, we need to schedule a cook.
    if ( bTransformChangeTriggersCooks )
    {
        // If we have landscape inputs in auto select components mode, we need to mark them
        // as changed so to recommit the landscape properly with updated transforms
        bool NeedLandscapeUpdate = false;
        for (int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
        {
            // Retrieve input at this index.
            UHoudiniAssetInput * AssetInput = Inputs[InputIdx];
            if (!AssetInput || AssetInput->IsPendingKill())
                continue;

            if (!AssetInput->IsLandscapeUpdateNeededOnTransformChange())
                continue;

            NeedLandscapeUpdate = true;
            AssetInput->MarkChanged();
        }

        if (bLoadedComponent && !FHoudiniEngineUtils::IsValidNodeId(AssetId) && !bAssetIsBeingInstantiated)
            StartTaskAssetCookingManual();

        bComponentNeedsCook = true;
        StartHoudiniTicking();
    }
}

void 
UHoudiniAssetComponent::SetBakingBaseNameOverride( const FHoudiniGeoPartObject& GeoPartObject, const FString& BaseName )
{
    if( const FString* FoundOverride = BakeNameOverrides.Find( GeoPartObject ) )
    {
        // forget the last baked package since we changed the name
        if( *FoundOverride != BaseName )
        {
            BakedStaticMeshPackagesForParts.Remove( GeoPartObject );
        }
    }
    BakeNameOverrides.Add( GeoPartObject, BaseName );
}

bool 
UHoudiniAssetComponent::RemoveBakingBaseNameOverride( const FHoudiniGeoPartObject& GeoPartObject )
{
    return BakeNameOverrides.Remove( GeoPartObject ) > 0;
}

#endif  // WITH_EDITOR

FText
UHoudiniAssetComponent::GetBakeFolder() const
{
    // Empty indicates default - which is Content root
    if( BakeFolder.IsEmpty() )
    {
        return LOCTEXT( "Game", "/Game" );
    }
    return BakeFolder;
}

void
UHoudiniAssetComponent::SetBakeFolder( const FString& Folder )
{
    FText NewBakeFolder = FText::FromString( Folder );
    if( !NewBakeFolder.EqualTo( BakeFolder ) )
    {
        BakeFolder = NewBakeFolder;
        BakedStaticMeshPackagesForParts.Empty();
        BakedMaterialPackagesForIds.Empty();
    }
}

FText
UHoudiniAssetComponent::GetTempCookFolder() const
{
    // Empty indicates default
    if( TempCookFolder.IsEmpty() )
    {
        // Get runtime settings.
        const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
        check( HoudiniRuntimeSettings );

        return HoudiniRuntimeSettings->TemporaryCookFolder;
    }

    return TempCookFolder;
}

void
UHoudiniAssetComponent::SetTempCookFolder(const FString& Folder)
{
    FText NewTempCookFolder = FText::FromString(Folder);
    if (!NewTempCookFolder.EqualTo(TempCookFolder))
    {
        TempCookFolder = NewTempCookFolder;
    }
}

FString UHoudiniAssetComponent::GetBakingBaseName( const FHoudiniGeoPartObject& GeoPartObject ) const
{
    if( const FString* FoundOverride = BakeNameOverrides.Find( GeoPartObject ) )
    {
        return *FoundOverride;
    }
    if( GeoPartObject.HasCustomName() )
    {
        return GeoPartObject.PartName;
    }

    FString DisplayName;
    if (GetOwner())
        DisplayName = GetOwner()->GetName();
    else
        DisplayName = GetName();

    return FString::Printf( TEXT( "%s_%d_%d_%d_%d" ),
        *DisplayName,
        GeoPartObject.ObjectId, GeoPartObject.GeoId,
        GeoPartObject.PartId, GeoPartObject.SplitId );
}

FBoxSphereBounds
UHoudiniAssetComponent::CalcBounds( const FTransform & LocalToWorld ) const
{
    FBoxSphereBounds LocalBounds;
    FBox BoundingBox = GetAssetBounds();
    if ( BoundingBox.GetExtent() == FVector::ZeroVector )
        BoundingBox.ExpandBy( 1.0f );

    LocalBounds = FBoxSphereBounds( BoundingBox );
    // fix for offset bounds - maintain local bounds origin
    LocalBounds.TransformBy(LocalToWorld);

    const auto & LocalAttachedChildren = GetAttachChildren();
    for (int32 Idx = 0; Idx < LocalAttachedChildren.Num(); ++Idx)
    {
        if ( !LocalAttachedChildren[ Idx ] )
            continue;

        FBoxSphereBounds ChildBounds = LocalAttachedChildren[ Idx ]->CalcBounds( LocalToWorld );
        if ( !ChildBounds.ContainsNaN() )
            LocalBounds = LocalBounds + ChildBounds;
    }

    return LocalBounds;
}

void
UHoudiniAssetComponent::UpdateRenderingInformation()
{
    // Need to send this to render thread at some point.
    MarkRenderStateDirty();

    // Update physics representation right away.
    RecreatePhysicsState();

    // Changed GetAttachChildren to 'GetAllDescendants' due to HoudiniMeshSplitInstanceComponent
    // not propagating property changes to their own child StaticMeshComponents.
    TArray< USceneComponent * > LocalAttachChildren;
    GetChildrenComponents(true, LocalAttachChildren);
    for ( TArray< USceneComponent * >::TConstIterator Iter( LocalAttachChildren ); Iter; ++Iter )
    {
        USceneComponent * SceneComponent = *Iter;
        if( SceneComponent )
            SceneComponent->RecreatePhysicsState();
    }

    // Since we have new asset, we need to update bounds.
    UpdateBounds();
}

void
UHoudiniAssetComponent::PostLoadReattachComponents()
{
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( StaticMeshComponent && !StaticMeshComponent->IsPendingKill() )
            StaticMeshComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    }

    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* >::TIterator Iter( SplineComponents ); Iter; ++Iter )
    {
        UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
        if( HoudiniSplineComponent && HoudiniSplineComponent->IsValidLowLevel() )
            HoudiniSplineComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    }

    for ( TMap< FString, UHoudiniHandleComponent * >::TIterator Iter( HandleComponents ); Iter; ++Iter )
    {
        UHoudiniHandleComponent * HoudiniHandleComponent = Iter.Value();
        if ( HoudiniHandleComponent && !HoudiniHandleComponent->IsPendingKill() )
            HoudiniHandleComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    }

    for (TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >::TIterator Iter(LandscapeComponents); Iter; ++Iter)
    {
        ALandscapeProxy * HoudiniLandscape = Iter.Value().Get();
        if ( HoudiniLandscape && HoudiniLandscape->IsValidLowLevel() )
            HoudiniLandscape->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
    }
}


#if WITH_EDITOR

void
UHoudiniAssetComponent::OnComponentCreated()
{
    // This event will only be fired for native Actor and native Component.
    Super::OnComponentCreated();
    
    if ( !GetOwner() || !GetOwner()->GetWorld() )
        return;

    if ( StaticMeshes.Num() == 0 )
    {
        // Create Houdini logo static mesh and component for it.
        CreateStaticMeshHoudiniLogoResource( StaticMeshes );
    }

    // Create replacement material object.
    if ( !HoudiniAssetComponentMaterials )
    {
        HoudiniAssetComponentMaterials =
            NewObject< UHoudiniAssetComponentMaterials >(
                this, UHoudiniAssetComponentMaterials::StaticClass(), NAME_None, RF_Public | RF_Transactional );
    }

    // Subscribe to delegates.
    SubscribeEditorDelegates();
}

void
UHoudiniAssetComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
    // Release static mesh related resources.
    ReleaseObjectGeoPartResources( StaticMeshes );
    StaticMeshes.Empty();
    StaticMeshComponents.Empty();

    // Release all curve related resources.
    ClearCurves();

    // Destroy all parameters.
    ClearParameters();

    // Destroy all inputs.
    ClearInputs();

    // Destroy all instance inputs.
    ClearInstanceInputs();

    // Destroy all handles.
    ClearHandles();

    // Destroy all landscapes.
    ClearLandscapes();

    // Inform downstream assets that we are dieing.
    ClearDownstreamAssets();

    // Clear cook content temp file
    ClearCookTempFile();

    // Release all Houdini related resources.
    ResetHoudiniResources();

    // Unsubscribe from Editor events.
    UnsubscribeEditorDelegates();

    Super::OnComponentDestroyed( bDestroyingHierarchy );
}

void
UHoudiniAssetComponent::OnRegister()
{
    Super::OnRegister();

    // We need to recreate render states for loaded components.
    if ( bLoadedComponent )
    {
        // Static meshes.
        for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TIterator Iter( StaticMeshComponents ); Iter; ++Iter )
        {
            UStaticMeshComponent * StaticMeshComponent = Iter.Value();
            if ( StaticMeshComponent && !StaticMeshComponent->IsPendingKill() )
            {
                // Recreate render state.
                StaticMeshComponent->RecreateRenderState_Concurrent();

                // Need to recreate physics state.
                StaticMeshComponent->RecreatePhysicsState();
            }
        }

        // Instanced static meshes.
        for ( auto& InstanceInput : InstanceInputs )
        {
            if (!InstanceInput || InstanceInput->IsPendingKill())
                continue;

             // Recreate render state.
            InstanceInput->RecreateRenderStates();

            // Recreate physics state.
            InstanceInput->RecreatePhysicsStates();
        }
    }

    // We can now consider the asset as fully loaded
    bFullyLoaded = true;
}
#endif

bool
UHoudiniAssetComponent::ContainsHoudiniLogoGeometry() const
{
    return bContainsHoudiniLogoGeometry;
}

void
UHoudiniAssetComponent::CreateStaticMeshHoudiniLogoResource( TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshMap )
{
    if ( !bIsNativeComponent )
        return;

    // Create Houdini logo static mesh and component for it.
    FHoudiniGeoPartObject HoudiniGeoPartObject;
    StaticMeshMap.Add( HoudiniGeoPartObject, FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get() );
    CreateObjectGeoPartResources( StaticMeshMap );
    bContainsHoudiniLogoGeometry = true;
}

#if WITH_EDITOR
void
UHoudiniAssetComponent::PostLoad()
{
    Super::PostLoad();

    // If we are being cooked, we don't want to do anything
    if( GIsCookerLoadingPackage )
        return;

    // Only do PostLoad stuff if we are in the editor world
    if( UWorld* World = GetWorld() )
    {
        if( World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::Inactive )
        {
            return;
        }
    }
    else
    {
        // we aren't in _any_ world - how unusual
        return;
    }

    SanitizePostLoad();

    // We loaded a component which has no asset associated with it.
    if ( !HoudiniAsset && StaticMeshes.Num() <= 0)
    {
        // Set geometry to be Houdini logo geometry, since we have no other geometry.
        CreateStaticMeshHoudiniLogoResource( StaticMeshes );
        return;
    }

    // Show busy cursor.
    FScopedBusyCursor ScopedBusyCursor;

    if ( StaticMeshes.Num() > 0 )
    {
        CreateObjectGeoPartResources( StaticMeshes );
    }
    else
    {
        // Iff the only component our owner has is us, then we should show the logo mesh
        auto SceneComponents = GetOwner()->GetComponentsByClass( USceneComponent::StaticClass() );
        if ( SceneComponents.Num() == 1 )
        {
            CreateStaticMeshHoudiniLogoResource( StaticMeshes );
        }
    }

    // Perform post load initialization on parameters.
    PostLoadInitializeParameters();

    // Perform post load initialization on instance inputs.
    PostLoadInitializeInstanceInputs();

    // Post attach components to parent asset component.
    PostLoadReattachComponents();

    // Update mobility.
    // It'll be changed to static if we generated a landscape,
    // and if not, to movable if any of our children is movable
    UpdateMobility();

    // Need to update rendering information.
    UpdateRenderingInformation();

    // Force editor to redraw viewports.
    if ( GEditor )
        GEditor->RedrawAllViewports();

    // Update properties panel after instantiation.
    UpdateEditorProperties( false );
}
#endif

void
UHoudiniAssetComponent::Serialize( FArchive & Ar )
{
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    if ( !Ar.IsSaving() && !Ar.IsLoading() )
        return;

    // Serialize component flags.
    Ar << HoudiniAssetComponentFlagsPacked;

    // State of this component.
    EHoudiniAssetComponentState::Enum ComponentState = EHoudiniAssetComponentState::Invalid;

    if ( Ar.IsSaving() )
    {
        if ( FHoudiniEngineUtils::IsValidNodeId( AssetId ) )
        {
            // Asset has been previously instantiated.

            if ( HapiGUID.IsValid() )
            {
                // Asset is being re-cooked asynchronously.
                ComponentState = EHoudiniAssetComponentState::BeingCooked;
            }
            else
            {
                // We have no pending asynchronous cook requests.
                ComponentState = EHoudiniAssetComponentState::Instantiated;
            }
        }
        else
        {
            if ( HoudiniAsset )
            {
                // Asset has not been instantiated and therefore must have asynchronous instantiation
                // request in progress.
                ComponentState = EHoudiniAssetComponentState::None;
            }
            else
            {
                // Component is in invalid state (for example is a default class object).
                ComponentState = EHoudiniAssetComponentState::Invalid;
            }
        }
    }

    // Serialize format version.
    uint32 HoudiniAssetComponentVersion = Ar.CustomVer( FHoudiniCustomSerializationVersion::GUID );
    Ar << HoudiniAssetComponentVersion;

    // Serialize component state.
    SerializeEnumeration< EHoudiniAssetComponentState::Enum >( Ar, ComponentState );

    // Serialize scaling information and import axis.
    Ar << GeneratedGeometryScaleFactor;
    Ar << TransformScaleFactor;
    SerializeEnumeration< EHoudiniRuntimeSettingsAxisImport >( Ar, ImportAxis );

    // Serialize generated component GUID.
    Ar << ComponentGUID;

    // If component is in invalid state, we can skip the rest of serialization.
    if ( ComponentState == EHoudiniAssetComponentState::Invalid )
        return;

    // If we have no asset, we can stop.
    if ( !HoudiniAsset && Ar.IsSaving() )
        return;

    // Serialize Houdini asset.
    UHoudiniAsset * HoudiniSerializedAsset = nullptr;

    if ( Ar.IsSaving() )
        HoudiniSerializedAsset = HoudiniAsset;

    Ar << HoudiniSerializedAsset;

    if ( Ar.IsLoading() )
    {
        if ( Ar.IsTransacting() && HoudiniAsset != HoudiniSerializedAsset )
        {
            bTransactionAssetChange = true;
            PreviousTransactionHoudiniAsset = HoudiniAsset;
        }

        HoudiniAsset = HoudiniSerializedAsset;
    }

    // If we are going into playmode, save asset id.
    // NOTE: bIsPlayModeActive_Unused is known to have been mistakenly saved to disk as ON,
    // the following fixes that case - should only happen once when first loading
    if( Ar.IsLoading() && bIsPlayModeActive_Unused )
    {
        HAPI_NodeId TempId;
        Ar << TempId;
        bIsPlayModeActive_Unused = false;
    }

    // Serialization of default preset.
    Ar << DefaultPresetBuffer;

    // Serialization of preset.
    {
        bool bPresetSaved = false;

        if ( Ar.IsSaving() )
        {
            bPresetSaved = true;
            if ( FHoudiniEngineUtils::IsValidNodeId( AssetId ) )
            {
                FHoudiniEngineUtils::GetAssetPreset( AssetId, PresetBuffer );
            }
        }

        Ar << bPresetSaved;

        if ( bPresetSaved )
        {
            Ar << PresetBuffer;
        }
    }

    // Serialize parameters.
    SerializeParameters( Ar );

    // Serialize parameters name map.
    if ( HoudiniAssetComponentVersion >= VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP )
    {
        Ar << ParameterByName;
    }
    else
    {
        if ( Ar.IsLoading() )
        {
            ParameterByName.Empty();

            // Otherwise if we are loading an older serialization format, we can reconstruct parameters name map.
            for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
            {
                UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
                if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsPendingKill() )
                    ParameterByName.Add( HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter );
            }
        }
    }

    // Serialize inputs.
    SerializeInputs( Ar );

    // Serialize material replacements and material assignments.
    Ar << HoudiniAssetComponentMaterials;

    // Serialize geo parts and generated static meshes.
    Ar << StaticMeshes;
    Ar << StaticMeshComponents;

    // Serialize instance inputs (we do this after geometry loading as we might need it).
    SerializeInstanceInputs( Ar );

    // Serialize curves.
    Ar << SplineComponents;

    // Serialize handles.
    Ar << HandleComponents;

    // Serialize downstream asset connections.
    Ar << DownstreamAssetConnections;

    // Serialize Landscape/GeoPart map
    if ( HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_LANDSCAPES )
    {
        Ar << LandscapeComponents;
    }

    if( HoudiniAssetComponentVersion >=  VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BAKENAME_OVERRIDE )
    {
        Ar << BakeNameOverrides;
    }

    TArray<UPackage *> DirtyPackages;
    if (HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_COOK_TEMP_PACKAGES)
    {
        TMap<FString, FString> SavedPackages;
        if ( Ar.IsSaving() )
        {
            for ( TMap<FString, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryPackages); IterPackage; ++IterPackage )
            {
                if (!IterPackage.Value().IsValid())
                    continue;

                UPackage * Package = IterPackage.Value().Get();
                if ( !Package || UPackage::IsEmptyPackage( Package ) )
                    continue;

                if (Package->IsDirty())
                    DirtyPackages.Add(Package);

                FString sValue = Package->GetFName().ToString();

                FString sKey = IterPackage.Key();
                SavedPackages.Add( sKey, sValue );
            }
        }

        Ar << SavedPackages;

        if ( Ar.IsLoading() )
        {
            for ( TMap<FString, FString > ::TIterator IterPackage( SavedPackages ); IterPackage; ++IterPackage )
            {
                FString sKey = IterPackage.Key();
                FString PackageFile = IterPackage.Value();
                if (!FPackageName::DoesPackageExist(PackageFile))
                    continue;

                UPackage * Package = nullptr;
                if ( !PackageFile.IsEmpty() )
                {
                    Package = FindPackage(nullptr, *PackageFile);
                }

                if ( !Package )
                    continue;

                if ( Package->IsDirty() )
                    DirtyPackages.Add(Package);

                CookedTemporaryPackages.Add( sKey, Package );
            }
        }
    }

    if ( HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_COOK_TEMP_PACKAGES_MESH_AND_LAYERS )
    {
        // Temporary Mesh Packages
        TMap<FHoudiniGeoPartObject, FString> MeshPackages;
        if ( Ar.IsSaving() )
        {
            for ( TMap<FHoudiniGeoPartObject, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryStaticMeshPackages); IterPackage; ++IterPackage )
            {
                if ( !IterPackage.Value().IsValid() )
                    continue;

                UPackage * Package = IterPackage.Value().Get();
                if ( !Package || UPackage::IsEmptyPackage( Package ) )
                    continue;

                if (Package->IsDirty())
                    DirtyPackages.Add(Package);

                FString sValue = Package->GetFName().ToString();

                FHoudiniGeoPartObject Key = IterPackage.Key();
                MeshPackages.Add( Key, sValue );
            }
        }

        Ar << MeshPackages;

        if ( Ar.IsLoading() )
        {
            for ( TMap<FHoudiniGeoPartObject, FString > ::TIterator IterPackage( MeshPackages ); IterPackage; ++IterPackage )
            {
                FHoudiniGeoPartObject Key = IterPackage.Key();
                FString PackageFile = IterPackage.Value();
                if (!FPackageName::DoesPackageExist(PackageFile))
                    continue;

                UPackage * Package = nullptr;
                if ( !PackageFile.IsEmpty() )
                {
                    Package = FindPackage(nullptr, *PackageFile);
                }

                if ( !Package )
                    continue;

                if (Package->IsDirty())
                    DirtyPackages.Add(Package);

                CookedTemporaryStaticMeshPackages.Add( Key, Package );
            }
        }

        // Temporary Landscape Layers Packages
        TMap<FString, FHoudiniGeoPartObject> LayerPackages;
        if ( Ar.IsSaving() )
        {
            for ( TMap<TWeakObjectPtr< UPackage >, FHoudiniGeoPartObject > ::TIterator IterPackage( CookedTemporaryLandscapeLayers ); IterPackage; ++IterPackage )
            {
                if (!IterPackage.Key().IsValid())
                    continue;

                UPackage * Package = IterPackage.Key().Get();
                if ( !Package || UPackage::IsEmptyPackage( Package ) )
                    continue;

                if ( Package->IsDirty() )
                    DirtyPackages.Add(Package);

                FString sKey = Package->GetFName().ToString();

                FHoudiniGeoPartObject Value = IterPackage.Value();
                LayerPackages.Add( sKey, Value );
            }
        }

        Ar << LayerPackages;

        if ( Ar.IsLoading() )
        {
            for ( TMap<FString, FHoudiniGeoPartObject > ::TIterator IterPackage( LayerPackages ); IterPackage; ++IterPackage )
            {
                FHoudiniGeoPartObject Value = IterPackage.Value();
                FString PackageFile = IterPackage.Key();
                if ( !FPackageName::DoesPackageExist(PackageFile) )
                    continue;

                UPackage * Package = nullptr;
                if ( !PackageFile.IsEmpty() )
                {
                    Package = FindPackage(nullptr, *PackageFile);
                }

                if ( !Package )
                    continue;

                if ( Package->IsDirty() )
                    DirtyPackages.Add(Package);

                CookedTemporaryLandscapeLayers.Add( Package, Value );
            }
        }
    }

#if WITH_EDITOR
    if (DirtyPackages.Num() > 0)
    {
        if (Ar.IsSaving() && !Ar.IsTransacting())
        {
            // Save the dirty packages that we're still using
            const bool bCheckDirty = false;
            const bool bPromptToSave = false;
            FEditorFileUtils::PromptForCheckoutAndSave(DirtyPackages, bCheckDirty, bPromptToSave);
        }
    }
#endif

    if ( Ar.IsLoading() && bIsNativeComponent )
    {
        // This component has been loaded.
        bLoadedComponent = true;
        bFullyLoaded = false;
    }
}

void
UHoudiniAssetComponent::SetStaticMeshGenerationParameters( UStaticMesh * StaticMesh ) const
{
#if WITH_EDITOR
    if( !StaticMesh )
        return;

    // Make sure static mesh has a new lighting guid.
    StaticMesh->LightingGuid = FGuid::NewGuid();
    StaticMesh->LODGroup = NAME_None;

    // Set resolution of lightmap.
    StaticMesh->LightMapResolution = GeneratedLightMapResolution;

    // Set Bias multiplier for Light Propagation Volume lighting.
    StaticMesh->LpvBiasMultiplier = GeneratedLpvBiasMultiplier;

    // Set the global light map coordinate index if it looks valid
    if ( StaticMesh->RenderData.IsValid() && StaticMesh->RenderData->LODResources.Num() > 0)
    {
        int32 NumUVs = StaticMesh->RenderData->LODResources[0].GetNumTexCoords();
        if ( NumUVs > GeneratedLightMapCoordinateIndex )
        {
            StaticMesh->LightMapCoordinateIndex = GeneratedLightMapCoordinateIndex;
        }
    }

    // Set method for LOD texture factor computation.
    /* TODO_414
    //StaticMesh->bUseMaximumStreamingTexelRatio = bGeneratedUseMaximumStreamingTexelRatio;

    // Set distance where textures using UV 0 are streamed in/out.  - GOES ON COMPONENT
    // StaticMesh->StreamingDistanceMultiplier = GeneratedStreamingDistanceMultiplier;
    */

    // Add user data.
    for( int32 AssetUserDataIdx = 0; AssetUserDataIdx < GeneratedAssetUserData.Num(); ++AssetUserDataIdx )
        StaticMesh->AddAssetUserData( GeneratedAssetUserData[ AssetUserDataIdx ] );

    StaticMesh->CreateBodySetup();
    UBodySetup * BodySetup = StaticMesh->BodySetup;
    check( BodySetup );

    // Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
    BodySetup->bDoubleSidedGeometry = bGeneratedDoubleSidedGeometry;

    // Assign physical material for simple collision.
    BodySetup->PhysMaterial = GeneratedPhysMaterial;

    BodySetup->DefaultInstance.CopyBodyInstancePropertiesFrom(&DefaultBodyInstance);

    // Assign collision trace behavior.
    BodySetup->CollisionTraceFlag = GeneratedCollisionTraceFlag;

    // Assign walkable slope behavior.
    BodySetup->WalkableSlopeOverride = GeneratedWalkableSlopeOverride;

    // We want to use all of geometry for collision detection purposes.
    BodySetup->bMeshCollideAll = true;

#endif
}

const TArray< UHoudiniAssetInstanceInputField * >
UHoudiniAssetComponent::GetAllInstanceInputFields() const
{
    TArray< UHoudiniAssetInstanceInputField * > AllInstanceInputFields;

    // Duplicate instanced static mesh components.
    for (auto& InstanceInput : InstanceInputs)
    {
        if (!InstanceInput || InstanceInput->IsPendingKill())
            continue;

        const TArray< UHoudiniAssetInstanceInputField * > CurrentInstanceInputFields = InstanceInput->GetInstanceInputFields();
        for (auto currentInputField : CurrentInstanceInputFields)
            AllInstanceInputFields.Add(currentInputField);
    }

    return AllInstanceInputFields;
}

#if WITH_EDITOR

AActor *
UHoudiniAssetComponent::CloneComponentsAndCreateActor()
{
    // Display busy cursor.
    FScopedBusyCursor ScopedBusyCursor;

    ULevel * Level = GetHoudiniAssetActorOwner() ? GetHoudiniAssetActorOwner()->GetLevel() : nullptr;
    if ( !Level )
        Level = GWorld->GetCurrentLevel();

    AActor * Actor = NewObject< AActor >( Level, NAME_None );
    Actor->AddToRoot();

    USceneComponent * RootComponent =
        NewObject< USceneComponent >( Actor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional );

    RootComponent->SetMobility(EComponentMobility::Static);
    RootComponent->bVisualizeComponent = true;

    const FTransform & ComponentWorldTransform = GetComponentTransform();
    RootComponent->SetWorldLocationAndRotation(
        ComponentWorldTransform.GetLocation(),
        ComponentWorldTransform.GetRotation() );

    Actor->SetRootComponent( RootComponent );
    Actor->AddInstanceComponent( RootComponent );

    RootComponent->RegisterComponent();

    // Duplicate static mesh components.
    {
        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshes ); Iter; ++Iter )
        {
            FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
            UStaticMesh * StaticMesh = Iter.Value();
            if ( !StaticMesh || StaticMesh->IsPendingKill() )
                continue;

            // Retrieve referenced static mesh component.
            UStaticMeshComponent * StaticMeshComponent = LocateStaticMeshComponent( StaticMesh );
            if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
                continue;

            // Bake the referenced static mesh.
            UStaticMesh * OutStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
                StaticMesh, this, HoudiniGeoPartObject, EBakeMode::CreateNewAssets );

            if ( !OutStaticMesh || OutStaticMesh->IsPendingKill() )
                continue;

            FAssetRegistryModule::AssetCreated( OutStaticMesh );

            // Create static mesh component for baked mesh.
            UStaticMeshComponent * DuplicatedComponent =
                NewObject< UStaticMeshComponent >( Actor, UStaticMeshComponent::StaticClass(), NAME_None );//, RF_Transactional );

            if ( !DuplicatedComponent || DuplicatedComponent->IsPendingKill() )
                continue;

            Actor->AddInstanceComponent( DuplicatedComponent );

            DuplicatedComponent->SetStaticMesh( OutStaticMesh );
            DuplicatedComponent->SetVisibility( true );
            DuplicatedComponent->SetRelativeTransform( HoudiniGeoPartObject.TransformMatrix );

            // If this is a collision geo, we need to make it invisible.
            if ( HoudiniGeoPartObject.IsCollidable() )
            {
                DuplicatedComponent->SetVisibility( false );
                DuplicatedComponent->SetHiddenInGame( true );
                DuplicatedComponent->SetCollisionProfileName( FName( TEXT( "InvisibleWall" ) ) );
            }

            // Reapply the uproperties modified by attributes on the duplicated component
            FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( DuplicatedComponent, HoudiniGeoPartObject );

            DuplicatedComponent->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepRelativeTransform );
            DuplicatedComponent->RegisterComponent();
        }
    }

    // Duplicate instanced static mesh components.
    for ( auto& InstanceInput : InstanceInputs )
    {
        if ( InstanceInput && !InstanceInput->IsPendingKill() )
            InstanceInput->CloneComponentsAndAttachToActor( Actor );
    }

    return Actor;
}

bool
UHoudiniAssetComponent::IsCookingEnabled() const
{
    return FHoudiniEngine::Get().GetEnableCookingGlobal() && bEnableCooking;
}

void
UHoudiniAssetComponent::PostEditUndo()
{
    // We need to make sure that all mesh components in the maps are valid ones
    CleanUpAttachedStaticMeshComponents();

    // Check the cooked materials refer to something..
    bool bCookedContentNeedRecook = false;
    for ( TMap< FString, TWeakObjectPtr< UPackage > > ::TIterator IterPackage( CookedTemporaryPackages ); IterPackage; ++IterPackage )
    {
        if ( bCookedContentNeedRecook )
            break;

        UPackage * Package = IterPackage.Value().Get();
        if ( Package )
        {
            FString PackageName = Package->GetName();
            if ( !PackageName.IsEmpty() && ( PackageName != TEXT( "None" ) ) )
                continue;
        }

        bCookedContentNeedRecook = true;
    }

    // Check the cooked meshes refer to something..
    for ( TMap< FHoudiniGeoPartObject, TWeakObjectPtr< UPackage > > ::TIterator IterPackage( CookedTemporaryStaticMeshPackages ); IterPackage; ++IterPackage )
    {
        if ( bCookedContentNeedRecook )
            break;

        UPackage * Package = IterPackage.Value().Get();
        if ( Package )
        {
            FString PackageName = Package->GetName();
            if ( !PackageName.IsEmpty() && ( PackageName != TEXT("None") ) )
                continue;
        }

        bCookedContentNeedRecook = true;
    }

    // Check the cooked landscape layers refer to something..
    for ( TMap< TWeakObjectPtr< UPackage >, FHoudiniGeoPartObject >::TIterator IterPackage( CookedTemporaryLandscapeLayers ); IterPackage; ++IterPackage )
    {
        if ( bCookedContentNeedRecook )
            break;

        UPackage * Package = IterPackage.Key().Get();
        if ( Package )
        {
            FString PackageName = Package->GetName();
            if ( !PackageName.IsEmpty() && ( PackageName != TEXT("None") ) )
                continue;
        }

        bCookedContentNeedRecook = true;
    }


    if ( bCookedContentNeedRecook )
        StartTaskAssetCookingManual();

    Super::PostEditUndo();
}

void
UHoudiniAssetComponent::PostEditImport()
{
    Super::PostEditImport();

    AHoudiniAssetActor * CopiedActor = FHoudiniEngineUtils::LocateClipboardActor( GetOwner(), TEXT( "" ) );
    if ( CopiedActor )
        OnComponentClipboardCopy( CopiedActor->HoudiniAssetComponent );
}

void UHoudiniAssetComponent::SanitizePostLoad()
{
    AActor* Owner = GetOwner();

    for(auto Iter : Parameters)
    {
        UHoudiniAssetParameter* Param = Iter.Value;
        if( !Param || Param->IsPendingKill() )
        {
            // we have at least one bad parameter, clear them all
            FMessageLog("LoadErrors").Error(LOCTEXT("NullParameterFound", "Houdini Engine: Null parameter found, clearing parameters"))
                ->AddToken(FUObjectToken::Create(Owner));
            Parameters.Empty();
            ParameterByName.Empty();
            break;
        }
    }

    for(auto Input : Inputs)
    {
        if( !Input || Input->IsPendingKill() )
        {
            // we have at least one bad input, clear them all
            FMessageLog("LoadErrors").Error(LOCTEXT("NullInputFound", "Houdini Engine: Null input found, clearing inputs"))
                ->AddToken(FUObjectToken::Create(Owner));
            Inputs.Empty();
            break;
        }
    }

    // Patch any invalid material references
    for( auto& SMCElem : StaticMeshComponents )
    {
        UStaticMesh* SM = SMCElem.Key;
        UMeshComponent* SMC = SMCElem.Value;
        if ( !SM || SM->IsPendingKill() )
            continue;

        if ( !SMC || SMC->IsPendingKill() )
            continue;

        auto& StaticMeshMaterials = SM->StaticMaterials;
        for( int32 MaterialIdx = 0; MaterialIdx < StaticMeshMaterials.Num(); ++MaterialIdx )
        {
            if( nullptr == StaticMeshMaterials[ MaterialIdx ].MaterialInterface )
            {
                auto DefaultMI = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
                StaticMeshMaterials[ MaterialIdx ].MaterialInterface = DefaultMI;
                SMC->SetMaterial( MaterialIdx, DefaultMI );
            }
        }
    }

    /*
    // Fetch the dirty package that need to be reloaded
    TArray<UPackage *> DirtyPackages;
    for (TMap<FString, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryPackages); IterPackage; ++IterPackage)
    {
        if (!IterPackage.Value().IsValid())
            continue;

        UPackage * Package = IterPackage.Value().Get();
        if (!Package || UPackage::IsEmptyPackage(Package))
            continue;

        if (Package->IsDirty())
            DirtyPackages.Add(Package);
    }

    for (TMap<FHoudiniGeoPartObject, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryStaticMeshPackages); IterPackage; ++IterPackage)
    {
        if (!IterPackage.Value().IsValid())
            continue;

        UPackage * Package = IterPackage.Value().Get();
        if (!Package || UPackage::IsEmptyPackage(Package))
            continue;

        if (Package->IsDirty())
            DirtyPackages.Add(Package);
    }

    for (TMap<TWeakObjectPtr< UPackage >, FHoudiniGeoPartObject > ::TIterator IterPackage(CookedTemporaryLandscapeLayers); IterPackage; ++IterPackage)
    {
        if (!IterPackage.Key().IsValid())
            continue;

        UPackage * Package = IterPackage.Key().Get();
        if (!Package || UPackage::IsEmptyPackage(Package))
            continue;

        if (Package->IsDirty())
            DirtyPackages.Add(Package);
    }

    if (DirtyPackages.Num() > 0)
    {
        FlushAsyncLoading();

        FText ErrorMessage;
        UPackageTools::ReloadPackages(DirtyPackages, ErrorMessage, UPackageTools::EReloadPackagesInteractionMode::AssumePositive);
    }
    */
}

void
UHoudiniAssetComponent::PostInitProperties()
{
    Super::PostInitProperties();

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

    if ( HoudiniRuntimeSettings )
    {
        // Copy cooking defaults from settings.
        bEnableCooking = HoudiniRuntimeSettings->bEnableCooking;
        bUploadTransformsToHoudiniEngine = HoudiniRuntimeSettings->bUploadTransformsToHoudiniEngine;
        bTransformChangeTriggersCooks = HoudiniRuntimeSettings->bTransformChangeTriggersCooks;

        // Copy static mesh generation parameters from settings.
        bGeneratedDoubleSidedGeometry = HoudiniRuntimeSettings->bDoubleSidedGeometry;
        GeneratedPhysMaterial = HoudiniRuntimeSettings->PhysMaterial;
        DefaultBodyInstance = HoudiniRuntimeSettings->DefaultBodyInstance;
        GeneratedCollisionTraceFlag = HoudiniRuntimeSettings->CollisionTraceFlag;
        GeneratedLpvBiasMultiplier = HoudiniRuntimeSettings->LpvBiasMultiplier;
        GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
        GeneratedLightMapCoordinateIndex = HoudiniRuntimeSettings->LightMapCoordinateIndex;
        bGeneratedUseMaximumStreamingTexelRatio = HoudiniRuntimeSettings->bUseMaximumStreamingTexelRatio;
        GeneratedStreamingDistanceMultiplier = HoudiniRuntimeSettings->StreamingDistanceMultiplier;
        GeneratedWalkableSlopeOverride = HoudiniRuntimeSettings->WalkableSlopeOverride;
        GeneratedFoliageDefaultSettings = HoudiniRuntimeSettings->FoliageDefaultSettings;
        GeneratedAssetUserData = HoudiniRuntimeSettings->AssetUserData;
        GeneratedDistanceFieldResolutionScale = HoudiniRuntimeSettings->GeneratedDistanceFieldResolutionScale;
    }
}

#endif

bool
UHoudiniAssetComponent::LocateStaticMeshes(
    const FString & ObjectName,
    TMap< FString, TArray< FHoudiniGeoPartObject > > & InOutObjectsToInstance, bool bSubstring ) const
{
    // See if map has entry for this object name.
    if ( !InOutObjectsToInstance.Contains( ObjectName ) )
        InOutObjectsToInstance.Add( ObjectName, TArray< FHoudiniGeoPartObject >() );

    {
        // Get array entry for this object name.
        TArray< FHoudiniGeoPartObject > & Objects = InOutObjectsToInstance[ ObjectName ];

        // Go through all geo part objects and see if we have matches.
        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TConstIterator Iter( StaticMeshes ); Iter; ++Iter )
        {
            const FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
            UStaticMesh * StaticMesh = Iter.Value();

            if ( !StaticMesh || StaticMesh->IsPendingKill() )
                continue;

            if ( ObjectName.Len() > 0 )
            {
                if ( bSubstring && ObjectName.Len() >= HoudiniGeoPartObject.ObjectName.Len() )
                {
                    int32 Index = ObjectName.Find(
                        *HoudiniGeoPartObject.ObjectName,
                        ESearchCase::IgnoreCase,
                        ESearchDir::FromEnd, INDEX_NONE );

                    if ( ( Index != -1 ) && ( Index + HoudiniGeoPartObject.ObjectName.Len() == ObjectName.Len() ) )
                        Objects.Add( HoudiniGeoPartObject );
                }
                else if ( HoudiniGeoPartObject.ObjectName.Equals( ObjectName ) )
                {
                    Objects.Add( HoudiniGeoPartObject );
                }
            }
        }
    }

    // Sort arrays.
    for ( TMap< FString, TArray< FHoudiniGeoPartObject > >::TIterator Iter( InOutObjectsToInstance ); Iter; ++Iter )
    {
        TArray< FHoudiniGeoPartObject > & Objects = Iter.Value();
        Objects.Sort( FHoudiniGeoPartObjectSortPredicate() );
    }

    return InOutObjectsToInstance.Num() > 0;
}

bool
UHoudiniAssetComponent::LocateStaticMeshes(
    int32 ObjectToInstanceId,
    TArray< FHoudiniGeoPartObject > & InOutObjectsToInstance ) const
{
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TConstIterator Iter( StaticMeshes ); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
        if ( HoudiniGeoPartObject.ObjectId == ObjectToInstanceId )
        {
            // Check that this part isn't being instanced at the part level
            if ( !HoudiniGeoPartObject.HapiPartIsInstanced() )
                InOutObjectsToInstance.Add( HoudiniGeoPartObject );
        }
    }

    // Sort array.
    InOutObjectsToInstance.Sort( FHoudiniGeoPartObjectSortPredicate() );

    return InOutObjectsToInstance.Num() > 0;
}


FHoudiniGeoPartObject
UHoudiniAssetComponent::LocateGeoPartObject( UStaticMesh * StaticMesh ) const
{
    FHoudiniGeoPartObject GeoPartObject;
    if ( !StaticMesh || StaticMesh->IsPendingKill() )
        return GeoPartObject;

    const FHoudiniGeoPartObject * FoundGeoPartObject = StaticMeshes.FindKey( StaticMesh );
    if ( FoundGeoPartObject )
        GeoPartObject = *FoundGeoPartObject;

    return GeoPartObject;
}

bool
UHoudiniAssetComponent::IsPIEActive() const
{
#if WITH_EDITOR
    for( const FWorldContext& Context : GEngine->GetWorldContexts() )
    {
        if( Context.WorldType == EWorldType::PIE )
        {
            return true;
        }
    }
#endif
    return false;
}

#if WITH_EDITOR

void
UHoudiniAssetComponent::CreateCurves( const TArray< FHoudiniGeoPartObject > & FoundCurves )
{
    bool bCurveCreated = false;

    TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* > NewSplineComponents;
    for ( TArray< FHoudiniGeoPartObject >::TConstIterator Iter( FoundCurves ); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = *Iter;

        // Retrieve node id from geo part.
        HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId( AssetId );
        if ( NodeId == -1 )
        {
            // Invalid node id.
            continue;
        }

        if ( !HoudiniGeoPartObject.HasParameters( AssetId ) )
        {
            // We have no parameters on this curve.
            continue;
        }

        // We need to cook the spline node.
        FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), NodeId, nullptr);

        FString CurvePointsString;
        EHoudiniSplineComponentType::Enum CurveTypeValue = EHoudiniSplineComponentType::Bezier;
        EHoudiniSplineComponentMethod::Enum CurveMethodValue = EHoudiniSplineComponentMethod::CVs;
        int32 CurveClosed = 1;

        HAPI_AttributeInfo AttributeRefinedCurvePositions;
        FHoudiniApi::AttributeInfo_Init(&AttributeRefinedCurvePositions);

        TArray< float > RefinedCurvePositions;
        if ( !FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
            HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION,
            AttributeRefinedCurvePositions, RefinedCurvePositions ) )
        {
            continue;
        }

        if ( !AttributeRefinedCurvePositions.exists && RefinedCurvePositions.Num() > 0 )
            continue;

        // Transfer refined positions to position vector and perform necessary axis swap.
        TArray< FVector > CurveDisplayPoints;
        FHoudiniEngineUtils::ConvertScaleAndFlipVectorData( RefinedCurvePositions, CurveDisplayPoints );

        if ( !FHoudiniEngineUtils::HapiGetParameterDataAsString(
                NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT(""), CurvePointsString) ||
           !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
               NodeId, HAPI_UNREAL_PARAM_CURVE_TYPE, (int32) EHoudiniSplineComponentType::Bezier, (int32&) CurveTypeValue ) ||
           !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
               NodeId, HAPI_UNREAL_PARAM_CURVE_METHOD, (int32) EHoudiniSplineComponentMethod::CVs, (int32&) CurveMethodValue ) ||
           !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
               NodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed ) )
        {
            continue;
        }

        // Process coords string and extract positions.
        TArray< FVector > CurvePositions;
        FHoudiniEngineUtils::ExtractStringPositions( CurvePointsString, CurvePositions );

        // Check if this curve already exists.
        UHoudiniSplineComponent * HoudiniSplineComponent = LocateSplineComponent( HoudiniGeoPartObject );

        if ( HoudiniSplineComponent )
        {
            // The curve already exists, we can reuse it.
            // Remove it from old map.
            SplineComponents.Remove( HoudiniGeoPartObject );
        }
        else
        {
            // We need to create a new curve.
            HoudiniSplineComponent = NewObject< UHoudiniSplineComponent >(
                this, UHoudiniSplineComponent::StaticClass(),
                NAME_None, RF_Public | RF_Transactional );

            bCurveCreated = true;
        }

        // Set the GeoPartObject
        HoudiniSplineComponent->SetHoudiniGeoPartObject( HoudiniGeoPartObject );

        // If we have no parent, we need to re-attach.
        if ( !HoudiniSplineComponent->GetAttachParent() )
            HoudiniSplineComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );

        HoudiniSplineComponent->SetVisibility( true );

        // If component is not registered, register it.
        if ( !HoudiniSplineComponent->IsRegistered() )
            HoudiniSplineComponent->RegisterComponent();

        // Add to map of components.
        NewSplineComponents.Add( HoudiniGeoPartObject, HoudiniSplineComponent );

        // Transform the component by transformation provided by HAPI.
        HoudiniSplineComponent->SetRelativeTransform( HoudiniGeoPartObject.TransformMatrix );

        // Create Transform for the HoudiniSplineComponents
        TArray< FTransform > CurvePoints;
        CurvePoints.SetNumUninitialized(CurvePositions.Num());

        FTransform trans = FTransform::Identity;
        for (int32 n = 0; n < CurvePoints.Num(); n++)
        {
            trans.SetLocation(CurvePositions[n]);
            CurvePoints[n] = trans;
        }

        // Construct curve from available data.
        HoudiniSplineComponent->Construct(
            HoudiniGeoPartObject, CurvePoints, CurveDisplayPoints, CurveTypeValue,
            CurveMethodValue, ( CurveClosed == 1 ) );
    }

#if WITH_EDITOR
    // The editor caches the current selection visualizer, so we need to trick
    // and pretend the selection has changed so that the HSplineVisualizer can be drawn immediately
    if ( bCurveCreated && GUnrealEd )
        GUnrealEd->NoteSelectionChange();
#endif

    ClearCurves();
    SplineComponents = NewSplineComponents;
}

void
UHoudiniAssetComponent::CreateParameters()
{
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * > NewParameters;
    if( FHoudiniParamUtils::Build(AssetId, this, Parameters, NewParameters) )
    {
        bEditorPropertiesNeedFullUpdate = true;

        // Remove all unused parameters.
        ClearParameters();

        // Update parameters.
        Parameters = NewParameters;
        for ( auto& ParmPair : NewParameters )
        {
            UHoudiniAssetParameter* Param = ParmPair.Value;
            if( Param && !Param->IsPendingKill() )
                ParameterByName.Add( Param->GetParameterName(), Param );
        }
    }
}

void
UHoudiniAssetComponent::NotifyParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter )
{
    if ( bLoadedComponent && !FHoudiniEngineUtils::IsValidNodeId( AssetId ) && !bAssetIsBeingInstantiated )
        bLoadedComponentRequiresInstantiation = true;

    if ( HoudiniAssetParameter )
    {
        // Some parameter types won't require a full update of the editor panel
        // This will avoid breaking the current selection
        UClass* FoundClass = HoudiniAssetParameter->GetClass();
        /*
        if ( FoundClass->IsChildOf< UHoudiniAssetParameterFloat >()
            || FoundClass->IsChildOf< UHoudiniAssetParameterInt >()
            || FoundClass->IsChildOf< UHoudiniAssetParameterString >() )
        */
        if ( !FoundClass->IsChildOf< UHoudiniAssetInput >() )
            bEditorPropertiesNeedFullUpdate = false;
    }

    bParametersChanged = true;
    StartHoudiniTicking();
}

void
UHoudiniAssetComponent::NotifyHoudiniSplineChanged( UHoudiniSplineComponent * HoudiniSplineComponent )
{
    if ( bLoadedComponent && !FHoudiniEngineUtils::IsValidNodeId( AssetId ) && !bAssetIsBeingInstantiated )
        bLoadedComponentRequiresInstantiation = true;

    bParametersChanged = true;
    StartHoudiniTicking();
}

void
UHoudiniAssetComponent::UnmarkChangedParameters()
{
    if ( bParametersChanged )
    {
        for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
        {
            UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
            if ( !HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill() )
                continue;

            // If parameter has changed, unmark it.
            if ( HoudiniAssetParameter->HasChanged() )
                HoudiniAssetParameter->UnmarkChanged();
        }
    }
}

void
UHoudiniAssetComponent::UploadChangedParameters()
{
    bool Success = true;

    if ( bParametersChanged )
    {
        // Upload inputs.
        for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( Inputs ); IterInputs; ++IterInputs )
        {
            UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
            if ( !HoudiniAssetInput || HoudiniAssetInput->IsPendingKill() )
                continue;

            // If input has changed, upload it to HAPI.
            if ( HoudiniAssetInput->HasChanged() )
            {
                Success &= HoudiniAssetInput->UploadParameterValue();
            }
        }

        // Upload parameters.
        for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
        {
            UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
            if (!HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill())
                continue;

            // If parameter has changed, upload it to HAPI.
            if ( HoudiniAssetParameter->HasChanged() )
            {
                Success &= HoudiniAssetParameter->UploadParameterValue();
            }
        }
    }

    if( !Success )
    {
        HOUDINI_LOG_ERROR(TEXT("%s UploadChangedParameters failed"), GetOwner() ? *GetOwner()->GetName() : *GetName());
    }

    // We no longer have changed parameters.
    bParametersChanged = false;
}

void
UHoudiniAssetComponent::UpdateLoadedParameters()
{
    if (!FHoudiniEngineUtils::IsValidNodeId(AssetId))
        return;

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if (!HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill())
            continue;

        HoudiniAssetParameter->SetNodeId( AssetId );
    }
}

bool
UHoudiniAssetComponent::CreateHandles()
{
    if ( !FHoudiniEngineUtils::IsValidNodeId( AssetId ) )
    {
        // There's no Houdini asset, we can return.
        return false;
    }

    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
    if ( FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ) != HAPI_RESULT_SUCCESS )
        return false;

    FHandleComponentMap NewHandleComponents;

    // If we have handles.
    if ( AssetInfo.handleCount > 0 )
    {
        TArray< HAPI_HandleInfo > HandleInfos;
        HandleInfos.SetNumZeroed( AssetInfo.handleCount );
        for (int32 Idx = 0; Idx < HandleInfos.Num(); Idx++)
            FHoudiniApi::HandleInfo_Init(&(HandleInfos[Idx]));

        if ( FHoudiniApi::GetHandleInfo(
            FHoudiniEngine::Get().GetSession(), AssetId,
            &HandleInfos[ 0 ], 0, AssetInfo.handleCount ) != HAPI_RESULT_SUCCESS )
        {
            return false;
        }

        for ( int32 HandleIdx = 0; HandleIdx < AssetInfo.handleCount; ++HandleIdx )
        {
            // Retrieve handle info for this index.
            const HAPI_HandleInfo & HandleInfo = HandleInfos[ HandleIdx ];

            // If we do not have bindings, we can skip.
            if ( HandleInfo.bindingsCount <= 0 )
                continue;

            FString TypeName = TEXT( "" );
            EHoudiniHandleType HandleType = EHoudiniHandleType::Unsupported;
            {
                FHoudiniEngineString HoudiniEngineString( HandleInfo.typeNameSH );
                if( !HoudiniEngineString.ToFString( TypeName ) )
                {
                    continue;
                }

                if( TypeName.Equals( TEXT( HAPI_UNREAL_HANDLE_TRANSFORM ) ) )
                    HandleType = EHoudiniHandleType::Xform;
                else if( TypeName.Equals( TEXT( HAPI_UNREAL_HANDLE_BOUNDER ) ) )
                    HandleType = EHoudiniHandleType::Bounder;
            }

            FString HandleName = TEXT( "" );

            {
                FHoudiniEngineString HoudiniEngineString( HandleInfo.nameSH );
                if ( !HoudiniEngineString.ToFString( HandleName ) )
                    continue;
            }

            if( HandleType == EHoudiniHandleType::Unsupported )
            {
                HOUDINI_LOG_DISPLAY( TEXT( "%s: Unsupported Handle Type %s for handle %s" ), GetOwner() ? *(GetOwner()->GetName()) : *GetName(), *TypeName, *HandleName );
                continue;
            }

            UHoudiniHandleComponent * HandleComponent = nullptr;
            UHoudiniHandleComponent ** FoundHandleComponent = HandleComponents.Find( HandleName );

            if ( FoundHandleComponent )
            {
                HandleComponent = *FoundHandleComponent;

                // Remove so that it's not destroyed.
                HandleComponents.Remove( HandleName );
            }
            else
            {
                HandleComponent = NewObject< UHoudiniHandleComponent >(
                    this, UHoudiniHandleComponent::StaticClass(),
                    NAME_None, RF_Public | RF_Transactional );
            }

            if ( !HandleComponent )
                continue;

            // If we have no parent, we need to re-attach.
            if ( !HandleComponent->GetAttachParent() )
                HandleComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );

            HandleComponent->SetVisibility( true );

            // If component is not registered, register it.
            if ( !HandleComponent->IsRegistered() )
                HandleComponent->RegisterComponent();

            if ( HandleComponent->Construct( AssetId, HandleIdx, HandleName, HandleInfo, Parameters, HandleType ) )
                NewHandleComponents.Add( HandleName, HandleComponent );
        }
    }

    ClearHandles();
    HandleComponents = NewHandleComponents;

    return true;
}

void
UHoudiniAssetComponent::CreateInputs()
{
    if ( !FHoudiniEngineUtils::IsValidNodeId( AssetId ) )
    {
        // There's no Houdini asset, we can return.
        return;
    }

    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
    int32 InputCount = 0;
    if ( FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ) == HAPI_RESULT_SUCCESS
        && AssetInfo.hasEverCooked )
    {
        InputCount = AssetInfo.geoInputCount;
    }

    // We've already created the number of required inputs.
    if( Inputs.Num() == InputCount )
        return;

    // Resize our inputs to match the asset
    if( InputCount == 0 )
    {
        ClearInputs();
    }
    else
    {
        if( InputCount > Inputs.Num() )
        {
            int32 NumNewInputs = InputCount - Inputs.Num();
            for( int32 InputIdx = Inputs.Num(); InputIdx < InputCount; ++InputIdx )
                Inputs.Add( UHoudiniAssetInput::Create( this, InputIdx, AssetId ) );
        }
        else
        {
            // Must be fewer inputs
            for( int32 InputIdx = InputCount; InputIdx < Inputs.Num(); ++InputIdx )
            {
                Inputs[ InputIdx ]->ConditionalBeginDestroy();
            }
            Inputs.SetNum( InputCount );
        }
    }

}

void
UHoudiniAssetComponent::UpdateLoadedInputs( const bool& ForceRefresh )
{
    if ( !FHoudiniEngineUtils::IsValidNodeId(AssetId) )
        return;

    bool Success = true;
    for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( Inputs ); IterInputs; ++IterInputs )
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if (!HoudiniAssetInput || HoudiniAssetInput->IsPendingKill() )
            continue;

        HoudiniAssetInput->SetNodeId( AssetId );

        // If we're updating the input landscape data, we need to restore the original input geometry
        // to submit it as the source mesh
        EHoudiniAssetInputType::Enum InputType = HoudiniAssetInput->GetChoiceIndex();
        if (InputType == EHoudiniAssetInputType::LandscapeInput
            && HoudiniAssetInput->IsUpdatingInputLandscape() )
            //&& !HoudiniAssetInput->IsLandscapeAssetConnected() )
            FHoudiniLandscapeUtils::RestoreLandscapeFromFile( HoudiniAssetInput->GetLandscapeInput() );

        Success &= HoudiniAssetInput->ChangeInputType( HoudiniAssetInput->GetChoiceIndex(), ForceRefresh );
        Success &= HoudiniAssetInput->UploadParameterValue();
    }

    if( !Success )
    {
        HOUDINI_LOG_ERROR(TEXT("%s UpdateLoadedInputs failed"), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
    }
}

bool
UHoudiniAssetComponent::UpdateWaitingForUpstreamAssetsToInstantiate( bool bNotifyUpstreamAsset )
{
    bWaitingForUpstreamAssetsToInstantiate = false;

    // We first need to make sure all our asset inputs have been instantiated and reconnected.
    for ( auto LocalInput : Inputs )
    {
        if ( !LocalInput || LocalInput->IsPendingKill() )
            continue;

        bool bInputAssetNeedsInstantiation = LocalInput->DoesInputAssetNeedInstantiation();
        if ( !bInputAssetNeedsInstantiation )
            continue;

        bWaitingForUpstreamAssetsToInstantiate = true;

        if ( bNotifyUpstreamAsset )
        {
            UHoudiniAssetComponent * LocalInputAssetComponent = LocalInput->GetConnectedInputAssetComponent();
            if ( LocalInputAssetComponent && !LocalInputAssetComponent->IsPendingKill() )
                LocalInputAssetComponent->NotifyParameterChanged( nullptr );
        }
    }

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetInput* Input = Cast< UHoudiniAssetInput >( IterParams.Value() );
        if ( !Input || Input->IsPendingKill() )
            continue;

        bool bInputAssetNeedsInstantiation = Input->DoesInputAssetNeedInstantiation();
        if ( !bInputAssetNeedsInstantiation )
            continue;

        bWaitingForUpstreamAssetsToInstantiate = true;

        if ( bNotifyUpstreamAsset )
        {
            UHoudiniAssetComponent * LocalInputAssetComponent = Input->GetConnectedInputAssetComponent();
            if ( LocalInputAssetComponent && !LocalInputAssetComponent->IsPendingKill() )
                LocalInputAssetComponent->NotifyParameterChanged( nullptr );
        }
    }

    return bWaitingForUpstreamAssetsToInstantiate;
}

bool
UHoudiniAssetComponent::RefreshEditableNodesAfterLoad()
{
    // For some reason, we need to go through all the editable nodes once
    // To "Activate" them...
    HAPI_AssetInfo AssetInfo;
    FHoudiniApi::AssetInfo_Init(&AssetInfo);
    HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), false);

    // Retrieve information about each object contained within our asset.
    TArray< HAPI_ObjectInfo > ObjectInfos;
    if (!FHoudiniEngineUtils::HapiGetObjectInfos(AssetId, ObjectInfos))
        return false;

    // Iterate through all objects.
    for (int32 ObjectIdx = 0; ObjectIdx < ObjectInfos.Num(); ++ObjectIdx)
    {
        // Retrieve object at this index.
        const HAPI_ObjectInfo & ObjectInfo = ObjectInfos[ObjectIdx];

        // Get all the GeoInfos for the editable nodes
        int32 EditableNodeCount = 0;
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeChildNodeList(
            FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId,
            HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE,
            true, &EditableNodeCount), false);

        if (EditableNodeCount <= 0)
            continue;

        TArray< HAPI_NodeId > EditableNodeIds;
        EditableNodeIds.SetNumUninitialized(EditableNodeCount);
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
            FHoudiniEngine::Get().GetSession(), AssetId,
            EditableNodeIds.GetData(), EditableNodeCount), false);

        for (int nEditable = 0; nEditable < EditableNodeCount; nEditable++)
        {
            HAPI_GeoInfo CurrentEditableGeoInfo;
            FHoudiniApi::GeoInfo_Init(&CurrentEditableGeoInfo);
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetGeoInfo(
                FHoudiniEngine::Get().GetSession(),
                EditableNodeIds[nEditable],
                &CurrentEditableGeoInfo), false);

            if ( CurrentEditableGeoInfo.type != HAPI_GEOTYPE_CURVE )
                continue;

            FString NodePathTemp;
            if ( !FHoudiniEngineUtils::HapiGetNodePath( CurrentEditableGeoInfo.nodeId, AssetId, NodePathTemp ) )
                continue;

            // We need to refresh the spline corresponding to that node
            for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* >::TIterator Iter( SplineComponents ); Iter; ++Iter )
            {
                FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();

                // Get NodePath appends the partId to the node path, so we use contains instead of equals
                if ( HoudiniGeoPartObject.GetNodePath().Contains( NodePathTemp ) )
                {
                    // Update the Geo/Node Id
                    HoudiniGeoPartObject.GeoId = CurrentEditableGeoInfo.nodeId;

                    // Update the attached spline component too
                    UHoudiniSplineComponent * SplineComponent = Iter.Value();
                    if ( SplineComponent )
                        SplineComponent->SetHoudiniGeoPartObject( HoudiniGeoPartObject );
                }
            }
        }
    }

    return true;
}


void
UHoudiniAssetComponent::UploadLoadedCurves()
{
    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* >::TIterator Iter( SplineComponents ); Iter; ++Iter )
    {
        UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
        if ( !HoudiniSplineComponent )
            continue;

        HoudiniSplineComponent->UploadControlPoints();
    }
}

UHoudiniAssetInstanceInput* 
UHoudiniAssetComponent::LocateInstanceInput( const FHoudiniGeoPartObject& GeoPart ) const
{
    for ( UHoudiniAssetInstanceInput* InstanceInput : InstanceInputs )
    {
        if ( !InstanceInput || InstanceInput->IsPendingKill() )
            continue;

        // Verify path to instancer + various configuration flags
        if ( InstanceInput->GetGeoPartObject().GetNodePath() == GeoPart.GetNodePath() &&
            UHoudiniAssetInstanceInput::GetInstancerFlags(GeoPart).HoudiniAssetInstanceInputFlagsPacked ==
            InstanceInput->Flags.HoudiniAssetInstanceInputFlagsPacked )
        {
            return InstanceInput;
        }
    }
    return nullptr;
}

void
UHoudiniAssetComponent::CreateInstanceInputs( const TArray< FHoudiniGeoPartObject > & Instancers )
{
    TArray< UHoudiniAssetInstanceInput * > NewInstanceInputs;

    for ( const FHoudiniGeoPartObject& GeoPart : Instancers )
    {
        if ( GeoPart.IsVisible() )
        {
            // Check if this instance input already exists.
            UHoudiniAssetInstanceInput * HoudiniAssetInstanceInput = nullptr;

            UHoudiniAssetInstanceInput * FoundHoudiniAssetInstanceInput = LocateInstanceInput(GeoPart);
            if ( FoundHoudiniAssetInstanceInput && !FoundHoudiniAssetInstanceInput->IsPendingKill() )
            {
                // Input already exists, we can reuse it.
                HoudiniAssetInstanceInput = FoundHoudiniAssetInstanceInput;

                // Since this is the corresponding part, we will refresh the InstanceInput's GeoPart
                HoudiniAssetInstanceInput->SetGeoPartObject( GeoPart );

                // Remove it from old map.
                InstanceInputs.Remove( FoundHoudiniAssetInstanceInput );
            }
            else
            {
                // Otherwise we need to create new instance input.
                HoudiniAssetInstanceInput = UHoudiniAssetInstanceInput::Create( this, GeoPart );
            }

            if ( !HoudiniAssetInstanceInput || HoudiniAssetInstanceInput->IsPendingKill() )
            {
                // Invalid instance input.
                HOUDINI_LOG_WARNING( TEXT( "%s: Failed to create Instancer from part %s" ), GetOwner() ? *(GetOwner()->GetName()) : *GetName(), *GeoPart.GetNodePath() );
            }
            else
            {
                // Add input to new map.
                NewInstanceInputs.Add( HoudiniAssetInstanceInput );
                // Create or re-create this input.
                HoudiniAssetInstanceInput->CreateInstanceInput();
            }
        }
    }

    // Clear all the existing instance inputs and replace with the new
    ClearInstanceInputs();
    InstanceInputs = NewInstanceInputs;
}

void
UHoudiniAssetComponent::DuplicateParameters( UHoudiniAssetComponent * DuplicatedHoudiniComponent )
{
    if ( !DuplicatedHoudiniComponent || DuplicatedHoudiniComponent->IsPendingKill() )
        return;

    TMap< HAPI_ParmId, UHoudiniAssetParameter * > & InParameters = DuplicatedHoudiniComponent->Parameters;
    TMap< FString, UHoudiniAssetParameter * > & InParametersByName = DuplicatedHoudiniComponent->ParameterByName;

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        HAPI_ParmId HoudiniAssetParameterKey = IterParams.Key();
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if ( !HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill() )
            continue;

        if (HoudiniAssetParameterKey == -1)
            continue;

        // Duplicate parameter.
        UHoudiniAssetParameter * DuplicatedHoudiniAssetParameter = HoudiniAssetParameter->Duplicate( DuplicatedHoudiniComponent );
        if (!DuplicatedHoudiniAssetParameter || DuplicatedHoudiniAssetParameter->IsPendingKill())
            continue;

        // PIE does not like standalone flags.
        DuplicatedHoudiniAssetParameter->ClearFlags( RF_Standalone );
        DuplicatedHoudiniAssetParameter->SetHoudiniAssetComponent( DuplicatedHoudiniComponent );

        // For Input Parameters
        UHoudiniAssetInput* HoudiniAssetInputParameter = Cast< UHoudiniAssetInput >(HoudiniAssetParameter);
        UHoudiniAssetInput* DuplicatedHoudiniAssetInputParameter = Cast< UHoudiniAssetInput >(DuplicatedHoudiniAssetParameter);
        if(HoudiniAssetInputParameter && DuplicatedHoudiniAssetInputParameter)
        {
            // Invalidate the node ids on the duplicate so that new inputs will be created.
            DuplicatedHoudiniAssetInputParameter->InvalidateNodeIds();

            // We also need to duplicate the attached curves properly
            DuplicatedHoudiniAssetInputParameter->DuplicateCurves(HoudiniAssetInputParameter);
        }

        InParameters.Add( HoudiniAssetParameterKey, DuplicatedHoudiniAssetParameter );
        InParametersByName.Add(
            DuplicatedHoudiniAssetParameter->GetParameterName(),
            DuplicatedHoudiniAssetParameter );
    }
}

void
UHoudiniAssetComponent::DuplicateHandles( UHoudiniAssetComponent * SrcAssetComponent )
{
    if (!SrcAssetComponent || SrcAssetComponent->IsPendingKill())
        return;

    for ( auto const & SrcNameToHandle : SrcAssetComponent->HandleComponents )
    {
        if (!SrcNameToHandle.Value || SrcNameToHandle.Value->IsPendingKill())
            continue;

        // Duplicate spline component.
        UHoudiniHandleComponent * NewHandleComponent =
            DuplicateObject< UHoudiniHandleComponent >( SrcNameToHandle.Value, this );

        if ( NewHandleComponent && !NewHandleComponent->IsPendingKill() )
        {
            NewHandleComponent->SetFlags( RF_Transactional | RF_Public );
            NewHandleComponent->ResolveDuplicatedParameters( Parameters );
            HandleComponents.Add( SrcNameToHandle.Key, NewHandleComponent );
        }
    }
}

void
UHoudiniAssetComponent::DuplicateInputs( UHoudiniAssetComponent * DuplicatedHoudiniComponent )
{
    if (!DuplicatedHoudiniComponent || DuplicatedHoudiniComponent->IsPendingKill())
        return;

    TArray< UHoudiniAssetInput * > & InInputs = DuplicatedHoudiniComponent->Inputs;
    for ( int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx )
    {
        // Retrieve input at this index.
        UHoudiniAssetInput * AssetInput = Inputs[ InputIdx ];
        if (!AssetInput || AssetInput->IsPendingKill() )
            continue;

        // Duplicate input.
        UHoudiniAssetInput * DuplicateAssetInput = DuplicateObject( AssetInput, DuplicatedHoudiniComponent );
        if (!DuplicateAssetInput || DuplicateAssetInput->IsPendingKill() )
            continue;

        DuplicateAssetInput->SetHoudiniAssetComponent( DuplicatedHoudiniComponent );

        // Invalidate the node ids on the duplicate so that new inputs will be created.
        DuplicateAssetInput->InvalidateNodeIds();

        // We also need to duplicate the attached curves properly
        DuplicateAssetInput->DuplicateCurves(AssetInput);

        // PIE does not like standalone flags.
        DuplicateAssetInput->ClearFlags( RF_Standalone );

        InInputs.Add( DuplicateAssetInput );
    }
}

void
UHoudiniAssetComponent::DuplicateInstanceInputs( UHoudiniAssetComponent * DuplicatedHoudiniComponent, const TMap<UObject*, UObject*>& ReplacementMap )
{
    if ( !DuplicatedHoudiniComponent || DuplicatedHoudiniComponent->IsPendingKill() )
        return;

    auto& InInstanceInputs = DuplicatedHoudiniComponent->InstanceInputs;

    for ( auto& HoudiniAssetInstanceInput : InstanceInputs )
    {
        if ( !HoudiniAssetInstanceInput && HoudiniAssetInstanceInput->IsPendingKill() )
            continue;

        UHoudiniAssetInstanceInput * DuplicatedHoudiniAssetInstanceInput =
            UHoudiniAssetInstanceInput::Create( DuplicatedHoudiniComponent, HoudiniAssetInstanceInput );

        if ( !DuplicatedHoudiniAssetInstanceInput || DuplicatedHoudiniAssetInstanceInput->IsPendingKill() )
            continue;

        // PIE does not like standalone flags.
        DuplicatedHoudiniAssetInstanceInput->ClearFlags( RF_Standalone );

        InInstanceInputs.Add( DuplicatedHoudiniAssetInstanceInput );

        // remap our instanced objects (only necessary for unbaked assets)
        for( UHoudiniAssetInstanceInputField* InputField : DuplicatedHoudiniAssetInstanceInput->GetInstanceInputFields() )
        {
            if ( !InputField || InputField->IsPendingKill() )
                continue;

            InputField->FixInstancedObjects( ReplacementMap );
        }
    }
}

bool
UHoudiniAssetComponent::CreateAllLandscapes( const TArray< FHoudiniGeoPartObject > & FoundVolumes )
{
    if ( FoundVolumes.Num() <= 0 )
        return false;

    // Try to create a Landscape for each HeightData found
    TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> > NewLandscapes;

    FHoudiniCookParams HoudiniCookParams( this );
    HoudiniCookParams.StaticMeshBakeMode = FHoudiniCookParams::GetDefaultStaticMeshesCookMode();
    HoudiniCookParams.MaterialAndTextureBakeMode = FHoudiniCookParams::GetDefaultMaterialAndTextureCookMode();

    // Keep a map of the valid landscape, this is to ensure that none of the new/input landscapes will be destroyed when cleaning up the old map
    TArray<ALandscapeProxy *> ValidLandscapes;
    // See if we have any landscape input that is marked as needing an update
    TArray<ALandscapeProxy *> InputLandscapesToUpdate;
    // Get all the inputs, including the object merge inputs
    TArray< UHoudiniAssetInput * > AllInputs;
    GetInputs(AllInputs, true);
    for ( auto CurrentInput : AllInputs)
    {
        if ( !CurrentInput )
            continue;

        if ( CurrentInput->GetChoiceIndex() != EHoudiniAssetInputType::Enum::LandscapeInput )
            continue;

        ALandscapeProxy* InputLandscape = CurrentInput->GetLandscapeInput();
        if ( !InputLandscape || InputLandscape->IsPendingKill() )
            continue;

        ValidLandscapes.Add( InputLandscape );

        if ( CurrentInput->IsUpdatingInputLandscape() )
            InputLandscapesToUpdate.Add( CurrentInput->GetLandscapeInput() );
    }

    if ( !FHoudiniLandscapeUtils::CreateAllLandscapes( HoudiniCookParams, FoundVolumes, LandscapeComponents, NewLandscapes, InputLandscapesToUpdate ) )
        return false;

    // The asset needs to be static in order to attach the landscapes to it
    SetMobility( EComponentMobility::Static );

    for ( TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >::TIterator IterLandscape( NewLandscapes ); IterLandscape; ++IterLandscape )
    {
        ALandscapeProxy* NewLandscape = IterLandscape.Value().Get();
        if ( !NewLandscape || !NewLandscape->IsValidLowLevel() )
            continue;

        // Add the new landscape to the valid list to avoid its destruction if we updated it
        ValidLandscapes.Add( NewLandscape );

        // Attach the new landscapes to ourselves if we dont already own it
        if ( GetAttachChildren().Find( NewLandscape->GetRootComponent() ) == INDEX_NONE )
            NewLandscape->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

        FHoudiniGeoPartObject Heightfield = IterLandscape.Key();

        // Update the materials from our assignement/replacement and the materials assigned on the previous version of this landscape
        UpdateLandscapeMaterialsAssignementsAndReplacements( NewLandscape, Heightfield );

        // Replace any reference we might still have to the old landscape with the new one
        TWeakObjectPtr<ALandscapeProxy>* OldLandscapePtr = LandscapeComponents.Find( Heightfield );
        if ( !OldLandscapePtr)
            continue;

        ALandscapeProxy* OldLandscape = OldLandscapePtr->Get();
        if ( !OldLandscape || !OldLandscape->IsValidLowLevel() )
            continue;

        if ( OldLandscape != NewLandscape )
            FHoudiniLandscapeUtils::UpdateOldLandscapeReference( OldLandscape, NewLandscape );        
    }

    // Replace the old landscapes map with the new ones
    for (TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >::TIterator Iter( LandscapeComponents ); Iter; ++Iter)
    {
        ALandscapeProxy * HoudiniLandscape = Iter.Value().Get();
        if ( !HoudiniLandscape || HoudiniLandscape->IsPendingKill() || !HoudiniLandscape->IsValidLowLevel() )
            continue;

        if ( ValidLandscapes.Contains( HoudiniLandscape ) )
            continue;

        HoudiniLandscape->UnregisterAllComponents();
        HoudiniLandscape->Destroy();
    }

    LandscapeComponents.Empty();
    LandscapeComponents = NewLandscapes;

    return true;
}

void UHoudiniAssetComponent::UpdateLandscapeMaterialsAssignementsAndReplacements(ALandscapeProxy* Landscape, FHoudiniGeoPartObject Heightfield )
{
    if ( !Landscape )
        return;

    // Handle the material assignment/replacement here
    UMaterialInterface* LandscapeMaterial = Landscape->GetLandscapeMaterial();
    if ( LandscapeMaterial )
    {
        // Make sure this material is in the assignements before trying to replacing it.
        if ( !GetAssignmentMaterial( LandscapeMaterial->GetName() ) && HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
            HoudiniAssetComponentMaterials->Assignments.Add( LandscapeMaterial->GetName(), LandscapeMaterial );

        // See if we have a replacement material for this.
        UMaterialInterface * ReplacementMaterialInterface = GetReplacementMaterial( Heightfield, LandscapeMaterial->GetName() );
        if ( ReplacementMaterialInterface )
            LandscapeMaterial = ReplacementMaterialInterface;
    }

    UMaterialInterface* LandscapeHoleMaterial = Landscape->GetLandscapeHoleMaterial();
    if ( LandscapeHoleMaterial )
    {
        // Make sure this material is in the assignemets before trying to replacing it.
        if ( !GetAssignmentMaterial( LandscapeHoleMaterial->GetName() ) && HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
            HoudiniAssetComponentMaterials->Assignments.Add( LandscapeHoleMaterial->GetName(), LandscapeHoleMaterial );

        // See if we have a replacement material for this.
        UMaterialInterface * ReplacementMaterialInterface = GetReplacementMaterial( Heightfield, LandscapeHoleMaterial->GetName() );
        if ( ReplacementMaterialInterface )
            LandscapeHoleMaterial = ReplacementMaterialInterface;
    }

    // Try to see if we can find materials used by the previous landscape for this Heightfield
    // ( the user might have replaced the landscape materials manually without us knowing it ) 
    if ( LandscapeComponents.Contains( Heightfield ) )
    {
        ALandscapeProxy* PreviousLandscape = LandscapeComponents[ Heightfield ].Get();
        if ( PreviousLandscape && PreviousLandscape->IsValidLowLevel() )
        {
            // Get the previously used materials, but ignore the default ones
            UMaterialInterface* PreviousLandscapeMaterial = PreviousLandscape->GetLandscapeMaterial();
            if ( PreviousLandscapeMaterial == UMaterial::GetDefaultMaterial( MD_Surface ) )
                PreviousLandscapeMaterial = nullptr;

            if ( PreviousLandscapeMaterial && PreviousLandscapeMaterial != LandscapeMaterial )
            {
                ReplaceMaterial( Heightfield, PreviousLandscapeMaterial, LandscapeMaterial, 0 );
                LandscapeMaterial = PreviousLandscapeMaterial;
            }

            // Do the same thing for the hole material
            UMaterialInterface* PreviousLandscapeHoleMaterial = PreviousLandscape->GetLandscapeHoleMaterial();
            if ( PreviousLandscapeHoleMaterial == UMaterial::GetDefaultMaterial( MD_Surface ) )
                PreviousLandscapeHoleMaterial = nullptr;

            if ( PreviousLandscapeHoleMaterial && PreviousLandscapeHoleMaterial != LandscapeHoleMaterial )
            {
                ReplaceMaterial( Heightfield, PreviousLandscapeHoleMaterial, LandscapeHoleMaterial, 0 );
                LandscapeHoleMaterial = PreviousLandscapeHoleMaterial;
            }
        }
    }

    // Assign the new material if they have been updated
    if ( Landscape->LandscapeMaterial != LandscapeMaterial )
        Landscape->LandscapeMaterial = LandscapeMaterial;

    if ( Landscape->LandscapeHoleMaterial != LandscapeHoleMaterial )
        Landscape->LandscapeHoleMaterial = LandscapeHoleMaterial;

    Landscape->UpdateAllComponentMaterialInstances();

    /*
    // As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
    // to trigger a fake Property change event that will call the Update function...
    UProperty* FoundProperty = FindField< UProperty >(Landscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
    if (FoundProperty)
    {
        FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
        Landscape->PostEditChangeProperty(PropChanged);
    }
    */
}

bool
UHoudiniAssetComponent::ReplaceLandscapeInInputs(ALandscapeProxy* Old, ALandscapeProxy* New )
{
    bool bReturn = false;

    // First, get all the inputs, including the object path parameters
    TArray< UHoudiniAssetInput * > AllInputs;
    GetInputs( AllInputs, true );

    // Look for landscape input, selecting the old landscape
    for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( AllInputs ); IterInputs; ++IterInputs )
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if ( !HoudiniAssetInput || HoudiniAssetInput->IsPendingKill() )
            continue;

        if ( HoudiniAssetInput->GetChoiceIndex() != EHoudiniAssetInputType::LandscapeInput )
            continue;

        if ( Old != HoudiniAssetInput->GetLandscapeInput() )
            continue;

        HoudiniAssetInput->OnLandscapeActorSelected( New );
        HoudiniAssetInput->UploadParameterValue();

        bReturn = true;
    }

    return bReturn;
}

#endif

void
UHoudiniAssetComponent::ClearInstanceInputs()
{
    for ( auto& InstanceInput : InstanceInputs )
    {
        if ( InstanceInput && !InstanceInput->IsPendingKill() )
            InstanceInput->ConditionalBeginDestroy();
    }

    InstanceInputs.Empty();
}

void
UHoudiniAssetComponent::ClearCurves()
{
    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter( SplineComponents ); Iter; ++Iter )
    {
        UHoudiniSplineComponent * SplineComponent = Iter.Value();
        if (SplineComponent)
        {
            SplineComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
            SplineComponent->UnregisterComponent();
            SplineComponent->DestroyComponent();
        }        
    }

    SplineComponents.Empty();
}

void
UHoudiniAssetComponent::ClearLandscapes()
{
    for (TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >::TIterator Iter(LandscapeComponents); Iter; ++Iter)
    {
        ALandscapeProxy * HoudiniLandscape = Iter.Value().Get();
        if ( !IsValid( HoudiniLandscape ) || !HoudiniLandscape->IsValidLowLevel() )
            continue;

        // Make sure we never destroy an input landscape
        bool IsCurrentLandscapeAnInput = false;
        for (auto CurrentInput : Inputs)
        {
            if (!CurrentInput)
                continue;

            if (CurrentInput->GetChoiceIndex() != EHoudiniAssetInputType::Enum::LandscapeInput)
                continue;

            ALandscapeProxy* InputLandscape = CurrentInput->GetLandscapeInput();
            if (!InputLandscape || InputLandscape->IsPendingKill())
                continue;

            IsCurrentLandscapeAnInput = true;
            break;
        }

        if ( IsCurrentLandscapeAnInput )
            continue;

        HoudiniLandscape->UnregisterAllComponents();
        HoudiniLandscape->Destroy();
    }

    LandscapeComponents.Empty();
}

void
UHoudiniAssetComponent::ClearParameters()
{
    for ( auto Iter : Parameters )
    {
        if ( Iter.Value && !Iter.Value->IsPendingKill() )
        {
            Iter.Value->ConditionalBeginDestroy();
        }
        else if(GetWorld() != NULL && GetWorld()->WorldType != EWorldType::PIE)
        {
            // Avoid spamming that error when leaving PIE mode
            HOUDINI_LOG_WARNING(TEXT("%s: null parameter when clearing"), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
        }
    }

    Parameters.Empty();
    ParameterByName.Empty();
}

void
UHoudiniAssetComponent::ClearHandles()
{
    for ( auto & NameToComponent : HandleComponents )
    {
        UHoudiniHandleComponent * HandleComponent = NameToComponent.Value;

        HandleComponent->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
        HandleComponent->UnregisterComponent();
        HandleComponent->DestroyComponent();
    }

    HandleComponents.Empty();
}

void
UHoudiniAssetComponent::ClearInputs()
{
    for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( Inputs ); IterInputs; ++IterInputs )
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if ( !HoudiniAssetInput || HoudiniAssetInput->IsPendingKill() )
            continue;

        if (HoudiniAssetInput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
            continue;

        // Destroy connected Houdini asset.
        HoudiniAssetInput->ConditionalBeginDestroy();
    }

    Inputs.Empty();
}

void
UHoudiniAssetComponent::ClearDownstreamAssets()
{
    for ( TMap< UHoudiniAssetComponent *, TSet< int32 > >::TIterator IterAssets(DownstreamAssetConnections );
        IterAssets; ++IterAssets )
    {
        UHoudiniAssetComponent * DownstreamAsset = IterAssets.Key();
        if (!DownstreamAsset || DownstreamAsset->IsPendingKill())
            continue;

        TSet< int32 > & LocalInputIndicies = IterAssets.Value();
        for ( auto LocalInputIndex : LocalInputIndicies )
        {
            if( DownstreamAsset->Inputs.IsValidIndex( LocalInputIndex )
                && DownstreamAsset->Inputs[ LocalInputIndex ] != nullptr
                && !DownstreamAsset->Inputs[LocalInputIndex]->IsPendingKill() )
            {
                DownstreamAsset->Inputs[ LocalInputIndex ]->ExternalDisconnectInputAssetActor();
            }
            else
            {
                // It could be connected as an operator path parameter
                bool DidADisconnect = false;
                for( auto& OtherParmElem : DownstreamAsset->Parameters )
                {
                    UHoudiniAssetInput* OtherParm = Cast<UHoudiniAssetInput>(OtherParmElem.Value);
                    if (!OtherParm || OtherParm->IsPendingKill())
                        continue;

                    if( OtherParm->GetConnectedInputAssetComponent() == this && OtherParm->IsInputAssetConnected() )
                    {
                        OtherParm->ExternalDisconnectInputAssetActor();
                        DidADisconnect = true;
                    }
                }

                if ( !DidADisconnect )
                    HOUDINI_LOG_ERROR( TEXT( "%s: Invalid downstream asset connection" ), GetOwner() ? *(GetOwner()->GetName()) : *GetName());
            }
        }
    }

    DownstreamAssetConnections.Empty();
}

void
UHoudiniAssetComponent::ClearCookTempFile()
{
    // First, Clean up the assignement/replacement map
    if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
        HoudiniAssetComponentMaterials->ResetMaterialInfo();

    // Then delete all the materials
    for ( TMap<FString, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryPackages );
        IterPackage; ++IterPackage)
    {
        UPackage * Package = IterPackage.Value().Get();
        if ( !Package )
            continue;

        Package->ClearFlags( RF_Standalone );
        //Package->ConditionalBeginDestroy();
    }

    CookedTemporaryPackages.Empty();

    // Delete all cooked Static Meshes
    for ( TMap<FHoudiniGeoPartObject, TWeakObjectPtr< UPackage > > ::TIterator IterPackage( CookedTemporaryStaticMeshPackages );
        IterPackage; ++IterPackage )
    {
        UPackage * Package = IterPackage.Value().Get();
        if ( !Package )
            continue;

        Package->ClearFlags( RF_Standalone );
        //Package->ConditionalBeginDestroy();
    }

    CookedTemporaryStaticMeshPackages.Empty();

    // Delete all cooked Landscape Layers
    for ( TMap<TWeakObjectPtr< UPackage >, FHoudiniGeoPartObject > ::TIterator IterPackage(CookedTemporaryLandscapeLayers);
        IterPackage; ++IterPackage )
    {
        UPackage * Package = IterPackage.Key().Get();
        if ( !Package )
            continue;

        Package->ClearFlags( RF_Standalone );
        //Package->ConditionalBeginDestroy();
    }

    CookedTemporaryLandscapeLayers.Empty();
}

UStaticMesh *
UHoudiniAssetComponent::LocateStaticMesh( const FHoudiniGeoPartObject & HoudiniGeoPartObject, const bool& ExactSearch ) const
{
    UStaticMesh * const * FoundStaticMesh = StaticMeshes.Find( HoudiniGeoPartObject );
    UStaticMesh * StaticMesh = nullptr;

    if ( !ExactSearch && !FoundStaticMesh )
    {
        // We couldnt find the exact SM corresponding to this geo part
        // Try again without caring for the split id
        for (TMap< FHoudiniGeoPartObject, UStaticMesh * >::TConstIterator IterSM( StaticMeshes ); IterSM; ++IterSM )
        {
            const FHoudiniGeoPartObject& HGPO = IterSM.Key();
            if (HGPO.AssetId == HoudiniGeoPartObject.AssetId && HGPO.ObjectId == HoudiniGeoPartObject.ObjectId
                && HGPO.GeoId == HoudiniGeoPartObject.GeoId && HGPO.PartId == HoudiniGeoPartObject.PartId)
            {
                FoundStaticMesh = &IterSM.Value();
                break;
            }
        }
    }

    if ( FoundStaticMesh )
        StaticMesh = *FoundStaticMesh;

    if ( !StaticMesh || StaticMesh->IsPendingKill() )
        return nullptr;

    return StaticMesh;
}

UStaticMeshComponent *
UHoudiniAssetComponent::LocateStaticMeshComponent( const UStaticMesh * StaticMesh ) const
{
    UStaticMeshComponent * const * FoundStaticMeshComponent = StaticMeshComponents.Find( StaticMesh );
    UStaticMeshComponent * StaticMeshComponent = nullptr;

    if ( FoundStaticMeshComponent )
        StaticMeshComponent = *FoundStaticMeshComponent;

    if ( StaticMeshComponent && StaticMeshComponent->IsPendingKill() )
        return nullptr;        

    return StaticMeshComponent;
}

bool
UHoudiniAssetComponent::LocateInstancedStaticMeshComponents(
    const UStaticMesh * StaticMesh, TArray< UInstancedStaticMeshComponent * > & Components ) const
{
    Components.Empty();

    bool bResult = false;

    for ( auto& InstanceInput : InstanceInputs )
    {
        if ( InstanceInput && !InstanceInput->IsPendingKill() )
            bResult |= InstanceInput->CollectAllInstancedStaticMeshComponents( Components, StaticMesh );
    }

    return bResult;
}

UHoudiniSplineComponent*
UHoudiniAssetComponent::LocateSplineComponent(const FHoudiniGeoPartObject & HoudiniGeoPartObject) const
{
    UHoudiniSplineComponent * const * FoundHoudiniSplineComponent = SplineComponents.Find(HoudiniGeoPartObject);
    UHoudiniSplineComponent * SplineComponent = nullptr;

    if ( FoundHoudiniSplineComponent )
        SplineComponent = *FoundHoudiniSplineComponent;

    if ( !SplineComponent || SplineComponent->IsPendingKill() || !SplineComponent->IsValidLowLevel() )
        return nullptr;

    return SplineComponent;
}

// Allow searching of geopart in array by names and not guid
UStaticMesh *
UHoudiniAssetComponent::LocateStaticMeshByNames(const FHoudiniGeoPartObject & HoudiniGeoPartObject, const TMap< FHoudiniGeoPartObject, UStaticMesh * >& FindInMap) const
{
    UStaticMesh * StaticMesh = nullptr;

    // Find via the object and Part names.
    for (TMap< FHoudiniGeoPartObject, UStaticMesh * >::TConstIterator IterSM(FindInMap); IterSM; ++IterSM)
    {
        const FHoudiniGeoPartObject& HGPO = IterSM.Key();
        if (HGPO.CompareNames(HoudiniGeoPartObject))
        {
            StaticMesh = IterSM.Value();
            break;
        }
    }

    if (!StaticMesh || StaticMesh->IsPendingKill())
        return nullptr;

    return StaticMesh;
}

void
UHoudiniAssetComponent::SerializeInputs( FArchive & Ar )
{
    if ( Ar.IsLoading() )
    {
        if ( !Ar.IsTransacting() )
            ClearInputs();
    }

    // We have to make sure that inputs are NOT saved with an empty name, as this will cause UE to crash on load
    for (TArray< UHoudiniAssetInput * >::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if (!HoudiniAssetInput || HoudiniAssetInput->IsPendingKill() )
            continue;

        if (HoudiniAssetInput->GetFName() != NAME_None)
            continue;

        // Calling Rename with null parameters will make sure the instanced input has a unique name
        HoudiniAssetInput->Rename();
    }

    Ar << Inputs;

    if ( Ar.IsTransacting() )
    {
        for (int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
        {
            UHoudiniAssetInput * HoudiniAssetInput = Inputs[InputIdx];
            if ( HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill() )
                HoudiniAssetInput->Serialize(Ar);
        }
    }
    else if ( Ar.IsLoading() )
    {
        for ( int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx )
        {
            UHoudiniAssetInput * HoudiniAssetInput = Inputs[ InputIdx ];
            if (HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill())
                Inputs[ InputIdx ]->SetHoudiniAssetComponent( this );
        }
    }
}

void
UHoudiniAssetComponent::SerializeInstanceInputs( FArchive & Ar )
{
    if ( Ar.IsLoading() )
    {
        // When loading for undo, we want to call Serialize on each InstanceInput
        if ( !Ar.IsTransacting() )
            ClearInstanceInputs();
        
        int32 HoudiniAssetComponentVersion = Ar.CustomVer( FHoudiniCustomSerializationVersion::GUID );
        if ( HoudiniAssetComponentVersion > VER_HOUDINI_ENGINE_COMPONENT_PARAMETER_NAME_MAP )
        {
            Ar << InstanceInputs;
        }
        else
        {
            int32 InstanceInputCount = 0;
            Ar << InstanceInputCount;

            InstanceInputs.SetNumUninitialized( InstanceInputCount );

            for ( int32 InstanceInputIdx = 0; InstanceInputIdx < InstanceInputCount; ++InstanceInputIdx )
            {
                HAPI_NodeId HoudiniInstanceInputKey = -1;

                Ar << HoudiniInstanceInputKey;
                Ar << InstanceInputs[ InstanceInputIdx ];
            }
        }
    } 
    else
    {
        // We have to make sure that instanced inputs are NOT saved with an empty name, as this will cause UE to crash on load
        for ( TArray< UHoudiniAssetInstanceInput * >::TIterator IterInstance(InstanceInputs ); IterInstance; ++IterInstance )
        {
            UHoudiniAssetInstanceInput * HoudiniInstanceInput = *IterInstance;
            if ( !HoudiniInstanceInput || HoudiniInstanceInput->IsPendingKill() )
                continue;

            if ( HoudiniInstanceInput->GetFName() != NAME_None )
                continue;

            // Calling Rename with null parameters will make sure the instanced input has a unique name
            HoudiniInstanceInput->Rename();
        }

        Ar << InstanceInputs;
    }

    if ( Ar.IsTransacting() )
    {
        for ( UHoudiniAssetInstanceInput* InstanceInput : InstanceInputs )
        {
            if ( InstanceInput && !InstanceInput->IsPendingKill() )
                InstanceInput->Serialize( Ar );
        }
    }
}

void
UHoudiniAssetComponent::SerializeParameters( FArchive & Ar )
{
    // We have to make sure that parameter are NOT saaved with an empty name, as this will cause UE to crash on load
    for (TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams(Parameters); IterParams; ++IterParams)
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if (!HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill() )
            continue;

        if (HoudiniAssetParameter->GetFName() != NAME_None)
            continue;

        // Calling Rename with null parameters will make sure the parameter has a unique name
        HoudiniAssetParameter->Rename();
    }

    Ar << Parameters;
}

void
UHoudiniAssetComponent::PostLoadInitializeInstanceInputs()
{
    for ( auto& InstanceInput : InstanceInputs )
    {
        if  (InstanceInput && !InstanceInput->IsPendingKill() )
            InstanceInput->SetHoudiniAssetComponent( this );
    }
}

void
UHoudiniAssetComponent::PostLoadInitializeParameters()
{
    // We need to re-patch parent parameter links.
    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if (!HoudiniAssetParameter || HoudiniAssetParameter->IsPendingKill())
            continue;

        HoudiniAssetParameter->SetHoudiniAssetComponent( this );

        HAPI_ParmId ParentParameterId = HoudiniAssetParameter->GetParmParentId();
        if ( ParentParameterId != -1 )
        {
            UHoudiniAssetParameter * const * FoundParentParameter = Parameters.Find( ParentParameterId );
            if (FoundParentParameter)
            {
                if ( *FoundParentParameter && !(*FoundParentParameter)->IsPendingKill() )
                    HoudiniAssetParameter->SetParentParameter(*FoundParentParameter);
            }
        }
    }
}

void
UHoudiniAssetComponent::RemoveStaticMeshComponent( UStaticMesh * StaticMesh )
{
    UStaticMeshComponent * const * FoundStaticMeshComponent = StaticMeshComponents.Find( StaticMesh );
    if ( FoundStaticMeshComponent )
    {
        StaticMeshComponents.Remove( StaticMesh );

        UStaticMeshComponent * StaticMeshComponent = *FoundStaticMeshComponent;
        if ( StaticMeshComponent && !StaticMeshComponent->IsPendingKill() )
        {
            StaticMeshComponent->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
            StaticMeshComponent->UnregisterComponent();
            StaticMeshComponent->DestroyComponent();
        }
    }
}

const FGuid &
UHoudiniAssetComponent::GetComponentGuid() const
{
    return ComponentGUID;
}

UMaterialInterface *
UHoudiniAssetComponent::GetReplacementMaterial(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    const FString & MaterialName )
{
    UMaterialInterface * ReplacementMaterial = nullptr;

    if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
    {
        TMap< FHoudiniGeoPartObject, TMap< FString, UMaterialInterface * > > & MaterialReplacements =
            HoudiniAssetComponentMaterials->Replacements;

        if ( MaterialReplacements.Contains( HoudiniGeoPartObject ) )
        {
            TMap< FString, UMaterialInterface * > & FoundReplacements = MaterialReplacements[ HoudiniGeoPartObject ];

            UMaterialInterface * const * FoundReplacementMaterial = FoundReplacements.Find( MaterialName );
            if ( FoundReplacementMaterial )
                ReplacementMaterial = *FoundReplacementMaterial;
        }
    }

    return ReplacementMaterial;
}

bool
UHoudiniAssetComponent::GetReplacementMaterialShopName(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    UMaterialInterface * MaterialInterface, FString & MaterialName)
{
    if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
    {
        TMap< FHoudiniGeoPartObject, TMap< FString, UMaterialInterface * > > & MaterialReplacements =
            HoudiniAssetComponentMaterials->Replacements;

        if ( MaterialReplacements.Contains( HoudiniGeoPartObject ) )
        {
            TMap< FString, UMaterialInterface * > & FoundReplacements = MaterialReplacements[ HoudiniGeoPartObject ];

            const FString * FoundMaterialShopName = FoundReplacements.FindKey( MaterialInterface );
            if ( FoundMaterialShopName )
            {
                MaterialName = *FoundMaterialShopName;
                return true;
            }
        }
    }

    return false;
}

UMaterialInterface *
UHoudiniAssetComponent::GetAssignmentMaterial( const FString & MaterialName )
{
    UMaterialInterface * Material = nullptr;

    if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
    {
        TMap< FString, UMaterialInterface * > & MaterialAssignments = HoudiniAssetComponentMaterials->Assignments;

        UMaterialInterface * const * FoundMaterial = MaterialAssignments.Find( MaterialName );
        if ( FoundMaterial )
            Material = *FoundMaterial;
    }

    return Material;
}

void UHoudiniAssetComponent::ClearAssignmentMaterials()
{
    if( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
    {
        HoudiniAssetComponentMaterials->Assignments.Empty();
    }
}

void 
UHoudiniAssetComponent::AddAssignmentMaterial( const FString& MaterialName, UMaterialInterface* MaterialInterface )
{
    if( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
    {
        HoudiniAssetComponentMaterials->Assignments.Add( MaterialName, MaterialInterface );
    }
}

bool
UHoudiniAssetComponent::ReplaceMaterial(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    UMaterialInterface * NewMaterialInterface,
    UMaterialInterface * OldMaterialInterface,
    int32 MaterialIndex )
{
    if ( !HoudiniAssetComponentMaterials || HoudiniAssetComponentMaterials->IsPendingKill() )
        return false;

    // Check that we do own this GeoPartObject, either via StaticMeshes or Landscapes
    UStaticMesh * StaticMesh = LocateStaticMesh( HoudiniGeoPartObject, false );
    if ( StaticMesh )
    {
        UStaticMeshComponent * StaticMeshComponent = LocateStaticMeshComponent( StaticMesh );
        if ( !StaticMeshComponent )
        {
            TArray< UInstancedStaticMeshComponent * > InstancedStaticMeshComponents;
            if ( !LocateInstancedStaticMeshComponents( StaticMesh, InstancedStaticMeshComponents ) )
                return false;
        }
    }
    else
    {
        if ( !LandscapeComponents.Find( HoudiniGeoPartObject ) )
            return false;
    }

    TMap< FHoudiniGeoPartObject, TMap< FString, UMaterialInterface * > > & MaterialReplacements =
        HoudiniAssetComponentMaterials->Replacements;

    TMap< FString, UMaterialInterface * > & MaterialAssignments = HoudiniAssetComponentMaterials->Assignments;

    UMaterialInterface * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();

    if ( !MaterialReplacements.Contains( HoudiniGeoPartObject ) )
    {
        // If there's no replacement map for this geo part object, add one.
        MaterialReplacements.Add( HoudiniGeoPartObject, TMap< FString, UMaterialInterface * >() );
    }

    // Retrieve replacements for this geo part object.
    TMap< FString, UMaterialInterface * > & MaterialReplacementsValues = MaterialReplacements[ HoudiniGeoPartObject ];

    const FString * FoundMaterialShopName = MaterialReplacementsValues.FindKey( OldMaterialInterface );
    if ( FoundMaterialShopName )
    {
        // This material has been replaced previously. Replace old material with new material.
        FString MaterialShopName = *FoundMaterialShopName;
        MaterialReplacementsValues.Add( MaterialShopName, NewMaterialInterface );
    }
    else
    {
        UMaterialInterface * OldMaterial = Cast< UMaterialInterface >( OldMaterialInterface );
        if ( OldMaterial )
        {
            // We have no previous replacement for this material, see if we have it in list of material assignments.
            FoundMaterialShopName = MaterialAssignments.FindKey( OldMaterial );
            if ( FoundMaterialShopName )
            {
                // This material has been assigned previously. Add material replacement entry.
                FString MaterialShopName = *FoundMaterialShopName;
                MaterialReplacementsValues.Add( MaterialShopName, NewMaterialInterface );
            }
            else if ( OldMaterial == DefaultMaterial )
            {
                // This is replacement for default material. Add material replacement entry.
                FString MaterialShopName = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
                MaterialReplacementsValues.Add( MaterialShopName, NewMaterialInterface );
            }
            else
            {
                // External Material?
                MaterialReplacementsValues.Add(OldMaterial->GetName(), NewMaterialInterface);
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

void
UHoudiniAssetComponent::SetMaterial( int32 ElementIndex, class UMaterialInterface* Material )
{
    Super::SetMaterial(ElementIndex, Material);

#if WITH_EDITOR
    UpdateEditorProperties(false);
#endif
}

void
UHoudiniAssetComponent::RemoveReplacementMaterial(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    const FString & MaterialName )
{
    if ( HoudiniAssetComponentMaterials && !HoudiniAssetComponentMaterials->IsPendingKill() )
    {
        TMap< FHoudiniGeoPartObject, TMap<FString, UMaterialInterface * > > & MaterialReplacements =
            HoudiniAssetComponentMaterials->Replacements;

        if ( MaterialReplacements.Contains( HoudiniGeoPartObject ) )
        {
            TMap< FString, UMaterialInterface * > & MaterialReplacementsValues =
                MaterialReplacements[ HoudiniGeoPartObject ];

            MaterialReplacementsValues.Remove( MaterialName );
        }
    }
}

bool
UHoudiniAssetComponent::CreateOrUpdateMaterialInstances()
{
#if WITH_EDITOR
    FHoudiniCookParams HoudiniCookParams( this );
    HoudiniCookParams.PackageGUID = ComponentGUID;
    HoudiniCookParams.MaterialAndTextureBakeMode = FHoudiniCookParams::GetDefaultMaterialAndTextureCookMode();

    bool bMaterialReplaced = false;

    // 1. FOR STATIC MESHES
    // Handling the creation of material instances from attributes    
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshes ); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
        UStaticMesh * StaticMesh = Iter.Value();

        // Invisible meshes are used for instancers, so we will not skip them as the material instance/parameter attributes
        // need to be set on the "source" mesh 
        //if ( !HoudiniGeoPartObject.IsVisible() )
        //    continue;

        if ( !StaticMesh || StaticMesh->IsPendingKill() )
            continue;

        // Replace the source material with the newly created/updated instance
        for( int32 MatIdx = 0; MatIdx < StaticMesh->StaticMaterials.Num(); MatIdx++ )
        {
            // The "source" material we want to create an instance of should have already been assigned to the mesh
            UMaterialInstance* NewMaterialInstance = nullptr;
            UMaterialInterface* SourceMaterialInterface = nullptr;

            // Create a new material instance if needed and update its parameter if needed
            if (!FHoudiniEngineMaterialUtils::CreateMaterialInstances(
                HoudiniGeoPartObject, HoudiniCookParams, NewMaterialInstance, SourceMaterialInterface, 
                HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE, MatIdx ) )
                continue;

            if (!NewMaterialInstance || !SourceMaterialInterface)
                continue;

            UMaterialInterface * SMMatInterface = StaticMesh->StaticMaterials[ MatIdx ].MaterialInterface;
            if ( SMMatInterface != SourceMaterialInterface && SMMatInterface->GetBaseMaterial() != SourceMaterialInterface )
                continue;

            // Replace the material assignment
            if ( !ReplaceMaterial( HoudiniGeoPartObject, NewMaterialInstance, SourceMaterialInterface, MatIdx ) )
                continue;

            // Update the StaticMesh, StaticMeshComponents and Instanced Static Mesh Components
            StaticMesh->Modify(); 
            StaticMesh->PreEditChange( nullptr );
            StaticMesh->StaticMaterials[ MatIdx ].MaterialInterface = NewMaterialInstance;            
            StaticMesh->PostEditChange();
            StaticMesh->MarkPackageDirty();

            UStaticMeshComponent * StaticMeshComponent = LocateStaticMeshComponent( StaticMesh );
            if ( StaticMeshComponent )
            {
                StaticMeshComponent->Modify();
                StaticMeshComponent->SetMaterial( MatIdx, NewMaterialInstance );

                bMaterialReplaced = true;
            }

            TArray< UInstancedStaticMeshComponent * > InstancedStaticMeshComponents;
            if ( LocateInstancedStaticMeshComponents( StaticMesh, InstancedStaticMeshComponents ) )
            {
                for ( int32 Idx = 0; Idx < InstancedStaticMeshComponents.Num(); ++Idx )
                {
                    UInstancedStaticMeshComponent * InstancedStaticMeshComponent = InstancedStaticMeshComponents[ Idx ];
                    if ( InstancedStaticMeshComponent )
                    {
                        InstancedStaticMeshComponent->Modify();
                        InstancedStaticMeshComponent->SetMaterial( MatIdx, NewMaterialInstance );

                        bMaterialReplaced = true;
                    }
                }
            }
        }
    }

    // 2. FOR LANDSCAPES
    // Handling the creation of material instances from attributes
    for ( TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >::TIterator Iter( LandscapeComponents ); Iter; ++Iter )
    {
        FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
        ALandscapeProxy* Landscape = Iter.Value().Get();
        if ( !Landscape || !Landscape->IsValidLowLevel() )
            continue;

        // The "source" landscape material we want to create an instance of should have already been assigned to the landscape
        UMaterialInstance* NewMaterialInstance = nullptr;
        UMaterialInterface* SourceMaterialInterface = nullptr;
        // Create/update a material instance if needed for the Landscape Material
        bool bLandscapeMaterialReplaced = false;
        if ( FHoudiniEngineMaterialUtils::CreateMaterialInstances(
            HoudiniGeoPartObject, HoudiniCookParams, NewMaterialInstance, SourceMaterialInterface, HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE ) )
        {
            if ( NewMaterialInstance && SourceMaterialInterface )
            {
                // Get the old material.
                UMaterialInterface * OldMaterial = Landscape->GetLandscapeMaterial();
                if ( OldMaterial == SourceMaterialInterface || OldMaterial->GetBaseMaterial() == SourceMaterialInterface )
                {
                    // Update our replacement table
                    bLandscapeMaterialReplaced = ReplaceMaterial( HoudiniGeoPartObject, OldMaterial, NewMaterialInstance, 0 );

                    // Update the landscape's material itself
                    Landscape->Modify();
                    Landscape->LandscapeMaterial = NewMaterialInstance;
                    bLandscapeMaterialReplaced = true;
                    bMaterialReplaced = true;
                }
            }
        }

        // Repeat the same process for the Landscape HoleMaterial        
        // The "source" hole material we want to create an instance of should have already been assigned to the landscape
        UMaterialInstance* NewHoleMaterialInstance = nullptr;
        UMaterialInterface* SourceHoleMaterialInterface = nullptr;
        // Create/update a material instance if needed for the Landscape Hole Material
        bool bLandscapeHoleMaterialReplaced = false;
        if ( FHoudiniEngineMaterialUtils::CreateMaterialInstances(
            HoudiniGeoPartObject, HoudiniCookParams, NewHoleMaterialInstance, SourceHoleMaterialInterface, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE_INSTANCE ) )
        {
            if ( NewHoleMaterialInstance && SourceHoleMaterialInterface )
            {
                // Get old material.
                UMaterialInterface * OldHoleMaterial = Landscape->GetLandscapeHoleMaterial();
                if (OldHoleMaterial == SourceHoleMaterialInterface || OldHoleMaterial->GetBaseMaterial() == SourceHoleMaterialInterface)
                {
                    // Update our replacement table
                    bLandscapeHoleMaterialReplaced = ReplaceMaterial(HoudiniGeoPartObject, OldHoleMaterial, NewHoleMaterialInstance, 0);

                    // Update the landscape's hole material
                    Landscape->Modify();
                    Landscape->LandscapeHoleMaterial = NewHoleMaterialInstance;
                    bLandscapeHoleMaterialReplaced = true;
                    bMaterialReplaced = true;
                }
            }
        }

        // For the landscape update:
        // As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
        // to trigger a fake Property change event that will call the Update function...
        if ( bLandscapeMaterialReplaced )
        {
            UProperty* FoundProperty = FindField< UProperty >( Landscape->GetClass(), TEXT( "LandscapeMaterial" ) );
            if ( FoundProperty )
            {
                FPropertyChangedEvent PropChanged( FoundProperty, EPropertyChangeType::ValueSet );
                Landscape->PostEditChangeProperty( PropChanged );
            }
        }

        if ( bLandscapeHoleMaterialReplaced )
        {
            UProperty* FoundProperty = FindField< UProperty >( Landscape->GetClass(), TEXT( "LandscapeHoleMaterial" ) );
            if ( FoundProperty )
            {
                FPropertyChangedEvent PropChanged( FoundProperty, EPropertyChangeType::ValueSet );
                Landscape->PostEditChangeProperty( PropChanged );
            }
        }
    }

    if ( bMaterialReplaced )
        UpdateEditorProperties( false );

    return bMaterialReplaced;
#else
    return false;
#endif
}

bool
UHoudiniAssetComponent::HasAnySockets() const
{
    // Return true if any of our StaticMeshComponent HasAnySocket
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
            continue;

        if ( StaticMeshComponent->HasAnySockets() )
            return true;
    }

    return Super::HasAnySockets();
}


/** Get a list of sockets this component contains   */
void 
UHoudiniAssetComponent::QuerySupportedSockets( TArray<FComponentSocketDescription>& OutSockets ) const
{
     // Query all the sockets in our StaticMeshComponents
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter(StaticMeshComponents); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
            continue;

        if ( !StaticMeshComponent->HasAnySockets() )
            continue;

        TArray< FComponentSocketDescription > ComponentSocket;
        StaticMeshComponent->QuerySupportedSockets( ComponentSocket );

        OutSockets.Append( ComponentSocket );
    }
}


bool 
UHoudiniAssetComponent::DoesSocketExist( FName SocketName ) const
{
    // Query all the sockets in our StaticMeshComponents
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
            continue;

        if ( StaticMeshComponent->DoesSocketExist( SocketName ) )
            return true;
    }
    
    return Super::DoesSocketExist( SocketName );
}


FTransform 
UHoudiniAssetComponent::GetSocketTransform( FName InSocketName, ERelativeTransformSpace TransformSpace ) const
{
    // Query all the sockets in our StaticMeshComponents
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent )
            continue;

        if ( !StaticMeshComponent->DoesSocketExist( InSocketName ) )
            continue;

        return StaticMeshComponent->GetSocketTransform( InSocketName, TransformSpace );
    }

    return Super::GetSocketTransform( InSocketName, TransformSpace );
}

FBox
UHoudiniAssetComponent::GetAssetBounds( UHoudiniAssetInput* IgnoreInput, const bool& bIgnoreGeneratedLandscape ) const
{
    FBox BoxBounds( ForceInitToZero );

    // Query the bounds of all our static mesh components..
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
            continue;

        FBox StaticMeshBounds = StaticMeshComponent->Bounds.GetBox();
        if ( StaticMeshBounds.IsValid )
            BoxBounds += StaticMeshBounds;
    }

    // Also scan all our decendants for SMC bounds not just top-level children
    // ( split mesh instances' mesh bounds were not gathered proiperly )
    TArray<USceneComponent*> LocalAttachedChildren;
    LocalAttachedChildren.Reserve(16);
    GetChildrenComponents(true, LocalAttachedChildren);
    for (int32 Idx = 0; Idx < LocalAttachedChildren.Num(); ++Idx)
    {
        if (!LocalAttachedChildren[Idx])
            continue;

        USceneComponent * pChild = LocalAttachedChildren[Idx];
        if (UStaticMeshComponent * StaticMeshComponent = Cast<UStaticMeshComponent>(pChild))
        {
            if (!StaticMeshComponent || StaticMeshComponent->IsPendingKill())
                continue;

            FBox StaticMeshBounds = StaticMeshComponent->Bounds.GetBox();
            if (StaticMeshBounds.IsValid)
                BoxBounds += StaticMeshBounds;
        }
    }

    //... all our Handles
    for ( TMap< FString, UHoudiniHandleComponent * >::TConstIterator Iter( HandleComponents ); Iter; ++Iter )
    {
        UHoudiniHandleComponent * HandleComponent = Iter.Value();
        if ( !HandleComponent )
            continue;

        BoxBounds += HandleComponent->GetComponentLocation();
    }

    // ... all our curves
    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent* >::TConstIterator Iter( SplineComponents ); Iter; ++Iter )
    {
        UHoudiniSplineComponent * SplineComponent = Iter.Value();
        if ( !SplineComponent || !SplineComponent->IsValidLowLevel() )
            continue;

        TArray<FVector> SplinePositions;
        SplineComponent->GetCurvePositions( SplinePositions );

        for (int32 n = 0; n < SplinePositions.Num(); n++)
        {
            BoxBounds += SplinePositions[ n ];
        }
    }
    
    // ... and inputs
    for ( int32 n = 0; n < Inputs.Num(); n++ )
    {
        UHoudiniAssetInput* CurrentInput = Inputs[ n ];
        if ( !CurrentInput || CurrentInput->IsPendingKill() )
            continue;

        if ( CurrentInput == IgnoreInput )
            continue;

        FBox StaticMeshBounds = CurrentInput->GetInputBounds( GetComponentLocation() );
        if ( StaticMeshBounds.IsValid )
            BoxBounds += StaticMeshBounds;
    }

    // ... all our landscapes
    if ( !bIgnoreGeneratedLandscape )
    {
        for (TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> >::TConstIterator Iter( LandscapeComponents ); Iter; ++Iter )
        {
            ALandscapeProxy * Landscape = Iter.Value().Get();
            if ( !Landscape )
                continue;

            FVector Origin, Extent;
            Landscape->GetActorBounds( false, Origin, Extent );

            FBox LandscapeBounds = FBox::BuildAABB( Origin, Extent );
            BoxBounds += LandscapeBounds;
        }
    }

    // If nothing was found, init with the asset's location
    if ( BoxBounds.GetVolume() == 0.0f )
        BoxBounds += GetComponentLocation();

    return BoxBounds;
}

bool UHoudiniAssetComponent::HasLandscapeActor(ALandscapeProxy* LandscapeActor ) const
{
    // Check if we created the landscape
    for (TMap<FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy>>::TConstIterator Iter(LandscapeComponents); Iter; ++Iter)
    {
        ALandscapeProxy * HoudiniLandscape = Iter.Value().Get();
        if ( HoudiniLandscape && HoudiniLandscape == LandscapeActor )
            return true;
    }

    return false;
}

TMap< FHoudiniGeoPartObject, TWeakObjectPtr<ALandscapeProxy> > *
UHoudiniAssetComponent::GetLandscapeComponents()
{
    return &LandscapeComponents;
}


/** Set the preset Input for HoudiniTools **/
void
UHoudiniAssetComponent::SetHoudiniToolInputPresets( const TMap<UObject*, int32>& InPresets )
{
#if WITH_EDITOR
    HoudiniToolInputPreset = InPresets;
#endif
}

#if WITH_EDITOR
void
UHoudiniAssetComponent::ApplyHoudiniToolInputPreset()
{
    if ( HoudiniToolInputPreset.Num() <= 0 )
        return;

    // We'll ignore inputs that have been preset to a curve type
    TArray< UHoudiniAssetInput*> InputArray;
    for ( auto CurrentInput : Inputs )
    {
        if (!CurrentInput || CurrentInput->IsPendingKill())
            continue;

        if ( CurrentInput->GetChoiceIndex() != EHoudiniAssetInputType::CurveInput )
            InputArray.Add( CurrentInput );
    }

    // Also look for ObjectPath parameter inputs
    for ( auto CurrentParam : Parameters )
    {
        UHoudiniAssetInput* CurrentInput = Cast< UHoudiniAssetInput > ( CurrentParam.Value );
        if ( !CurrentInput || CurrentInput->IsPendingKill() )
            continue;

        if ( CurrentInput->GetChoiceIndex() != EHoudiniAssetInputType::CurveInput )
            InputArray.Add( CurrentInput );
    }

    // Identify some special cases first
    bool OnlyHoudiniAssets = true;
    bool OnlyLandscapes = true;
    bool OnlyOneInput = true;
    for ( TMap< UObject*, int32 >::TIterator IterToolPreset( HoudiniToolInputPreset ); IterToolPreset; ++IterToolPreset )
    {
        UObject * Object = IterToolPreset.Key();
        AHoudiniAssetActor* HAsset = Cast<AHoudiniAssetActor>( Object );
        if ( !HAsset || HAsset->IsPendingKill() )
            OnlyHoudiniAssets = false;

        ALandscapeProxy* Landscape = Cast<ALandscapeProxy>( Object );
        if ( !Landscape || Landscape->IsPendingKill() )
            OnlyLandscapes = false;

        if ( IterToolPreset.Value() != 0 )
            OnlyOneInput = false;
    }

    /*
    if ( OnlyHoudiniAssets )
    {
        // Try to apply the supplied Object to the Input
        for (TMap< UObject*, int32 >::TIterator IterToolPreset( HoudiniToolInputPreset ); IterToolPreset; ++IterToolPreset)
        {
            AHoudiniAssetActor* HAsset = Cast< AHoudiniAssetActor >( IterToolPreset.Key() );
            if (!HAsset)
                continue;

            int32 InputNumber = IterToolPreset.Value();
            if ( !InputArray.IsValidIndex( InputNumber ) )
                continue;

            InputArray[ InputNumber ]->ChangeInputType( EHoudiniAssetInputType::AssetInput );
            InputArray[ InputNumber ]->OnInputActorSelected( HAsset );
        }
    }
    else if ( OnlyLandscapes )
    {
        // Try to apply the supplied Object to the Input
        for (TMap< UObject*, int32 >::TIterator IterToolPreset( HoudiniToolInputPreset ); IterToolPreset; ++IterToolPreset)
        {
            ALandscapeProxy* Landscape = Cast< ALandscape >( IterToolPreset.Key() );
            if ( !Landscape )
                continue;

            int32 InputNumber = IterToolPreset.Value();
            if ( !InputArray.IsValidIndex( InputNumber ) )
                continue;

            InputArray[ InputNumber ]->ChangeInputType( EHoudiniAssetInputType::LandscapeInput );
            InputArray[ InputNumber ]->OnLandscapeActorSelected( Landscape );
        }
    }
    else
    */
    {
        // Try to apply the supplied Object to the Input
        for (TMap< UObject*, int32 >::TIterator IterToolPreset( HoudiniToolInputPreset ); IterToolPreset; ++IterToolPreset)
        {
            UObject * Object = IterToolPreset.Key();
            if (!Object || Object->IsPendingKill())
                continue;

            int32 InputNumber = IterToolPreset.Value();

            if ( !InputArray.IsValidIndex( InputNumber ) )
                continue;

            InputArray[ InputNumber ]->AddInputObject( Object );

            if ( OnlyLandscapes && ( InputArray[ InputNumber ]->GetChoiceIndex() != EHoudiniAssetInputType::LandscapeInput ) )
                InputArray[ InputNumber ]->ChangeInputType( EHoudiniAssetInputType::LandscapeInput, true );

            // Dont auto select asset inputs because they dont have the unreal transform, which can cause issue, prefer WorldInput instead
            //if ( OnlyHoudiniAssets && ( InputArray[ InputNumber ]->GetChoiceIndex() != EHoudiniAssetInputType::AssetInput ) )
            //    InputArray[ InputNumber ]->ChangeInputType( EHoudiniAssetInputType::AssetInput );
        }
    }

    // Discard the tool presets after their first setup
    HoudiniToolInputPreset.Empty();
}
#endif

FPrimitiveSceneProxy*
UHoudiniAssetComponent::CreateSceneProxy()
{
    /** Represents a UHoudiniAssetComponent to the scene manager. */
    class FHoudiniAssetSceneProxy final : public FPrimitiveSceneProxy
    {
        public:
            SIZE_T GetTypeHash() const override
            {
                static size_t UniquePointer;
                return reinterpret_cast<size_t>( &UniquePointer );
            }

            FHoudiniAssetSceneProxy( const UHoudiniAssetComponent* InComponent )
                : FPrimitiveSceneProxy( InComponent )
            {
            }

            virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
            {
                FPrimitiveViewRelevance Result;
                Result.bDrawRelevance = IsShown( View );
                return Result;
            }

            virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
            uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }
    };

    return new FHoudiniAssetSceneProxy( this );
}

void
UHoudiniAssetComponent::NotifyAssetNeedsToBeReinstantiated()
{
    bLoadedComponentRequiresInstantiation = true;
    bLoadedComponent = true;
    bFullyLoaded = false;
    AssetCookCount = 0;
    AssetId = -1;

    // Mark all input as changed
    for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( Inputs ); IterInputs; ++IterInputs )
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if ( HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill() )
            HoudiniAssetInput->MarkChanged( false );
    }

    // Upload parameters.
    for (TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams(Parameters); IterParams; ++IterParams)
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsPendingKill() )
            HoudiniAssetParameter->MarkChanged( false );
    }
}

void
UHoudiniAssetComponent::UpdateMobility()
{
    // If we generated a landscape, force ourself to Static!
    if ( LandscapeComponents.Num() > 0 )
    {
        SetMobility(EComponentMobility::Static);
    }
    else
    {
        // Changed GetAttachChildren to 'GetAllDescendants' due to HoudiniMeshSplitInstanceComponent 
        // not propagating property changes to their own child StaticMeshComponents.
        TArray< USceneComponent * > LocalAttachChildren;
        GetChildrenComponents(true, LocalAttachChildren);

        // If one of the children we created is movable, we need to set ourselves to movable as well
        for (TArray< USceneComponent * >::TConstIterator Iter(LocalAttachChildren); Iter; ++Iter)
        {
            USceneComponent * SceneComponent = *Iter;
            if (SceneComponent->Mobility == EComponentMobility::Movable)
                SetMobility(EComponentMobility::Movable);
        }
    }
}


ELightMapInteractionType UHoudiniAssetComponent::GetStaticLightingType() const
{
    // Override preventing the automatic reset to default of LightmapType in UPrimitiveComponent::PostEditChangeProperty
    // see UStaticMeshComponent::GetStaticLightingType and UStaticMeshComponent::GetStaticLightingInfo
    switch (LightmapType)
    {    
        //default and ForceSurface are Texture mode.
        case ELightmapType::Default:
        case ELightmapType::ForceSurface:
            return LMIT_Texture;

        case ELightmapType::ForceVolumetric:
            return LMIT_GlobalVolume;
    }

    return LMIT_None;
}

void
UHoudiniAssetComponent::CopyComponentPropertiesTo(UPrimitiveComponent * pPrimComp)
{
    // Fixes UProperties not being propagated to newly created UStaticMeshComponents
    // Copies all of the properties that would normally get propagated.
    if (!pPrimComp || pPrimComp->IsPendingKill())
        return;

    pPrimComp->CastShadow = this->CastShadow;
    pPrimComp->bCastDynamicShadow = this->bCastDynamicShadow;
    pPrimComp->bCastStaticShadow = this->bCastStaticShadow;
    pPrimComp->bCastVolumetricTranslucentShadow = this->bCastVolumetricTranslucentShadow;
    pPrimComp->bCastInsetShadow = this->bCastInsetShadow;
    pPrimComp->bCastHiddenShadow = this->bCastHiddenShadow;
    pPrimComp->bCastShadowAsTwoSided = this->bCastShadowAsTwoSided;
    pPrimComp->LightmapType = this->LightmapType;
    pPrimComp->bLightAttachmentsAsGroup = this->bLightAttachmentsAsGroup;
    pPrimComp->IndirectLightingCacheQuality = this->IndirectLightingCacheQuality;
    pPrimComp->bVisibleInReflectionCaptures = this->bVisibleInReflectionCaptures;
    pPrimComp->bRenderInMainPass = this->bRenderInMainPass;
    //pPrimComp->bRenderInMono = this->bRenderInMono;
    pPrimComp->bOwnerNoSee = this->bOwnerNoSee;
    pPrimComp->bOnlyOwnerSee = this->bOnlyOwnerSee;
    pPrimComp->bTreatAsBackgroundForOcclusion = this->bTreatAsBackgroundForOcclusion;
    pPrimComp->bUseAsOccluder = this->bUseAsOccluder;
    pPrimComp->bRenderCustomDepth = this->bRenderCustomDepth;
    pPrimComp->CustomDepthStencilValue = this->CustomDepthStencilValue;
    pPrimComp->CustomDepthStencilWriteMask = this->CustomDepthStencilWriteMask;
    pPrimComp->TranslucencySortPriority = this->TranslucencySortPriority;
    pPrimComp->LpvBiasMultiplier = this->LpvBiasMultiplier;
    pPrimComp->bReceivesDecals = this->bReceivesDecals;
    pPrimComp->BoundsScale = this->BoundsScale;
    pPrimComp->bUseAttachParentBound = this->bUseAttachParentBound;
    pPrimComp->bAlwaysCreatePhysicsState = this->bAlwaysCreatePhysicsState;
    pPrimComp->bMultiBodyOverlap = this->bMultiBodyOverlap;
    //pPrimComp->bCheckAsyncSceneOnMove = this->bCheckAsyncSceneOnMove;
    pPrimComp->bTraceComplexOnMove = this->bTraceComplexOnMove;
    pPrimComp->bReturnMaterialOnMove = this->bReturnMaterialOnMove;
    pPrimComp->BodyInstance = this->BodyInstance;
    pPrimComp->CanCharacterStepUpOn = this->CanCharacterStepUpOn;
    pPrimComp->bIgnoreRadialImpulse = this->bIgnoreRadialImpulse;
    pPrimComp->bIgnoreRadialForce = this->bIgnoreRadialForce;
    pPrimComp->bApplyImpulseOnDamage = this->bApplyImpulseOnDamage;
    pPrimComp->MinDrawDistance = this->MinDrawDistance;
    pPrimComp->LDMaxDrawDistance = this->LDMaxDrawDistance;
    pPrimComp->CachedMaxDrawDistance = this->CachedMaxDrawDistance;
    pPrimComp->bAllowCullDistanceVolume = this->bAllowCullDistanceVolume;
    pPrimComp->DetailMode = this->DetailMode;
}

#undef LOCTEXT_NAMESPACE