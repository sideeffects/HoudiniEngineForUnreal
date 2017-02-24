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
*      Damian Campeanu, Mykola Konyk, Pavlo Penenko
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniApi.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
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
#include "Landscape.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Engine/StaticMeshSocket.h"
#include "MessageDialog.h"
#include "Widgets/Input/SButton.h"
#if WITH_EDITOR
#include "UnrealEdGlobals.h"
#include "FileHelpers.h"
#include "Editor/UnrealEdEngine.h"
#endif

#include "Internationalization.h"
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
        const auto & LocalAttachChildren = GetAttachChildren(); \
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
    Mobility = EComponentMobility::Movable;
    PrimaryComponentTick.bCanEverTick = true;
    bTickInEditor = true;
    bGenerateOverlapEvents = false;

    // Similar to UMeshComponent.
    CastShadow = true;
    bUseAsOccluder = true;
    bCanEverAffectNavigation = true;

    // This component requires render update.
    bNeverNeedsRenderUpdate = false;

    // Initialize static mesh generation parameters.
    bGeneratedDoubleSidedGeometry = false;
    GeneratedPhysMaterial = nullptr;
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
            Collector.AddReferencedObject( HoudiniAssetParameter, InThis );
        }

        // Add references to all inputs.
        for ( TArray< UHoudiniAssetInput * >::TIterator
            IterInputs( HoudiniAssetComponent->Inputs ); IterInputs; ++IterInputs )
        {
            UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
            Collector.AddReferencedObject( HoudiniAssetInput, InThis );
        }

        // Add references to all instance inputs.
        for ( auto& InstanceInput : HoudiniAssetComponent->InstanceInputs)
        {
            Collector.AddReferencedObject( InstanceInput, InThis );
        }

        // Add references to all handles.
        for ( TMap< FString, UHoudiniHandleComponent * >::TIterator
            IterHandles( HoudiniAssetComponent->HandleComponents ); IterHandles; ++IterHandles )
        {
            UHoudiniHandleComponent * HandleComponent = IterHandles.Value();
            Collector.AddReferencedObject( HandleComponent, InThis );
        }

        // Add references to all static meshes and corresponding geo parts.
        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator
            Iter( HoudiniAssetComponent->StaticMeshes ); Iter; ++Iter )
        {
            UStaticMesh * StaticMesh = Iter.Value();
            if ( StaticMesh )
                Collector.AddReferencedObject( StaticMesh, InThis );
        }

        // Add references to all static meshes and their static mesh components.
        for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TIterator
            Iter( HoudiniAssetComponent->StaticMeshComponents ); Iter; ++Iter )
        {
            UStaticMesh * StaticMesh = Iter.Key();
            UStaticMeshComponent * StaticMeshComponent = Iter.Value();

            Collector.AddReferencedObject( StaticMesh, InThis );
            Collector.AddReferencedObject( StaticMeshComponent, InThis );
        }

        // Add references to all spline components.
        for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * >::TIterator
            Iter( HoudiniAssetComponent->SplineComponents ); Iter; ++Iter )
        {
            UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
            Collector.AddReferencedObject( HoudiniSplineComponent, InThis );
        }

        // Add references to all Landscape
        for (TMap< FHoudiniGeoPartObject, ALandscape * >::TIterator
            Iter(HoudiniAssetComponent->LandscapeComponents); Iter; ++Iter)
        {
            ALandscape * HoudiniLandscape = Iter.Value();
            Collector.AddReferencedObject( HoudiniLandscape, InThis );
        }

        // Retrieve asset associated with this component and add reference to it.
        // Also do not add reference if it is being referenced by preview component.
        UHoudiniAsset * HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
        if ( HoudiniAsset && !HoudiniAssetComponent->bIsPreviewComponent )
        {
            // Manually add a reference to Houdini asset from this component.
            Collector.AddReferencedObject( HoudiniAsset, InThis );
        }

        // Add reference to material replacements.
        if ( HoudiniAssetComponent->HoudiniAssetComponentMaterials )
            Collector.AddReferencedObject( HoudiniAssetComponent->HoudiniAssetComponentMaterials, InThis );
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
    return FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId);
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
    if ( HoudiniAssetComponentMaterials )
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
    if ( InDownstreamAssetComponent )
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
    if ( DownstreamAssetConnections.Contains( InDownstreamAssetComponent ) )
    {
        TSet< int32 > & InputIndicesSet = DownstreamAssetConnections[ InDownstreamAssetComponent ];
        if ( InputIndicesSet.Contains( InInputIndex ) )
            InputIndicesSet.Remove( InInputIndex );
    }
}

void
UHoudiniAssetComponent::CreateObjectGeoPartResources( TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshMap )
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
            if ( HoudiniGeoPartObject.IsVisible() && !StaticMesh )
            {
                HOUDINI_LOG_WARNING( TEXT( "No static mesh generated for visible part %d,%d,%d" ), HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.PartId );
                continue;
            }

            UStaticMeshComponent * StaticMeshComponent = nullptr;
            UStaticMeshComponent * FoundStaticMeshComponent = LocateStaticMeshComponent( StaticMesh );

            if ( FoundStaticMeshComponent )
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
                    GetOwner(), UStaticMeshComponent::StaticClass(),
                    NAME_None, RF_Transactional );

                // Attach created static mesh component to our Houdini component.
                StaticMeshComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );

                StaticMeshComponent->SetStaticMesh( StaticMesh );
                StaticMeshComponent->SetVisibility( true );
                StaticMeshComponent->SetMobility( Mobility );
                StaticMeshComponent->RegisterComponent();

                // Add to the map of components.
                StaticMeshComponents.Add( StaticMesh, StaticMeshComponent );
            }

            if ( StaticMeshComponent )
            {
                // If this is a collision geo, we need to make it invisible.
                if (HoudiniGeoPartObject.IsCollidable())
                {
                    StaticMeshComponent->SetVisibility( false );
                    StaticMeshComponent->SetHiddenInGame( true );                                                                     
                    StaticMeshComponent->SetCollisionProfileName( FName( TEXT( "InvisibleWall" ) ) );
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
                    if ( MeshSocket && ( MeshSocket->Tag.IsEmpty() ) )
                        continue;

                    FHoudiniEngineUtils::AddActorsToMeshSocket( StaticMesh->Sockets[nSocket], StaticMeshComponent );
                }

                // Try to update uproperty atributes
                FHoudiniEngineUtils::UpdateUPropertyAttributes(StaticMeshComponent, HoudiniGeoPartObject);
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
    if (&StaticMeshes != &StaticMeshMap)
        StaticMeshes = StaticMeshMap;

#if WITH_EDITOR
    if ( FHoudiniEngineUtils::IsHoudiniAssetValid( AssetId ) )
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
}


void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources( bool bDeletePackages )
{
    ReleaseObjectGeoPartResources( StaticMeshes, bDeletePackages );
}

void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources(
    TMap< FHoudiniGeoPartObject, UStaticMesh * > & StaticMeshMap,
    bool bDeletePackages )
{
    // Record generated static meshes which we need to delete.
    TArray< UStaticMesh * > StaticMeshesToDelete;

    // Get Houdini logo.
    UStaticMesh * HoudiniLogoMesh = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh();

    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshMap ); Iter; ++Iter )
    {
        UStaticMesh * StaticMesh = Iter.Value();
        if ( !StaticMesh )
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
        if ( !StaticMeshComponent )
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
        if ( HoudiniAssetParameter && HoudiniAssetParameter->IsSubstanceParameter() )
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
        if ( HoudiniAssetParameter && HoudiniAssetParameter->IsA( ParameterClass ) )
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

    return HoudiniAssetParameter;
}

void
UHoudiniAssetComponent::GetAllUsedStaticMeshes( TArray< UStaticMesh * > & UsedStaticMeshes )
{
    UsedStaticMeshes.Empty();

    // Add all static meshes.
    for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( StaticMeshes ); Iter; ++Iter )
    {
        UStaticMesh * StaticMesh = Iter.Value();
        if ( StaticMesh )
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
        for ( const UHoudiniAssetInstanceInputField* InputField : InstanceInput->GetInstanceInputFields() )
        {
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
        for ( const UHoudiniAssetInstanceInputField* InputField : InstanceInput->GetInstanceInputFields() )
        {
            for ( int32 VarIndex = 0; VarIndex < InputField->InstanceVariationCount(); ++VarIndex )
            {
                UObject* Comp = InputField->GetInstancedComponent( VarIndex );
                if ( InputField->GetInstanceVariation( VarIndex ) && Comp->IsA<UHoudiniInstancedActorComponent>() )
                {
                    OutSMComponentToPart.Add( Cast<UHoudiniInstancedActorComponent>( Comp ), InputField->GetHoudiniGeoPartObject() );
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
    return ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) && ( 0 == AssetCookCount ) );
}

void
UHoudiniAssetComponent::AssignUniqueActorLabel()
{
    if ( GEditor && FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
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

    if (bCookError)
    {
        // We need to reset the manual recook flag here to avoid endless cooking
        bManualRecookRequested = false;
        return;
    }

    FTransform ComponentTransform;
    TMap< FHoudiniGeoPartObject, UStaticMesh * > NewStaticMeshes;
    if ( FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
        this, StaticMeshes, NewStaticMeshes, ComponentTransform) )
    {
        // Remove all duplicates. After this operation, old map will have meshes which we need
        // to deallocate.
        for ( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator
            Iter( NewStaticMeshes ); Iter; ++Iter )
        {
            const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
            UStaticMesh * StaticMesh = Iter.Value();

            // Removes the mesh from previous map of meshes
            UStaticMesh * FoundOldStaticMesh = LocateStaticMesh( HoudiniGeoPartObject );
            if ( ( FoundOldStaticMesh ) && ( FoundOldStaticMesh == StaticMesh ) )
            {
                // Mesh has not changed, we need to remove it from the old map to avoid deallocation.
                StaticMeshes.Remove( HoudiniGeoPartObject );
            }
        }

        // Make sure rendering is done
        FlushRenderingCommands();
        
        // Free meshes and components that are no longer used.
        ReleaseObjectGeoPartResources(StaticMeshes, true);

        // Set meshes and create new components for those meshes that do not have them.
        if ( NewStaticMeshes.Num() > 0 )
            CreateObjectGeoPartResources( NewStaticMeshes );
        else
            CreateStaticMeshHoudiniLogoResource( NewStaticMeshes );
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
            DownstreamAsset->bManualRecookRequested = true;
            DownstreamAsset->NotifyParameterChanged( nullptr );
        }
    }
}

void
UHoudiniAssetComponent::TickHoudiniComponent()
{
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

            switch( TaskInfo.TaskState )
            {
                case EHoudiniEngineTaskState::FinishedInstantiation:
                {
                    HOUDINI_LOG_MESSAGE( TEXT("    %s FinishedInstantiation." ), *GetOwner()->GetName() );

                    if ( FHoudiniEngineUtils::IsValidAssetId( TaskInfo.AssetId ) )
                    {
                        // Set new asset id.
                        SetAssetId( TaskInfo.AssetId );

                        // Assign unique actor label based on asset name.
                        AssignUniqueActorLabel();

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
                        HOUDINI_LOG_MESSAGE( TEXT( "    %s Received invalid asset id." ), *GetOwner()->GetName() );
                    }

                    break;
                }

                case EHoudiniEngineTaskState::FinishedCooking:
                {
                    HOUDINI_LOG_MESSAGE( TEXT( "   %s FinishedCooking." ), *GetOwner()->GetName() );

                    if ( FHoudiniEngineUtils::IsValidAssetId( TaskInfo.AssetId ) )
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
#endif
                    }
                    else
                    {
                        HOUDINI_LOG_MESSAGE( TEXT( "    %s Received invalid asset id." ), *GetOwner()->GetName() );
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
                    HOUDINI_LOG_MESSAGE( TEXT( "    %s FinishedCookingWithErrors." ), *GetOwner()->GetName() );

                    if ( FHoudiniEngineUtils::IsValidAssetId( TaskInfo.AssetId ) )
                    {
                        // Call post cook event with error parameter. This will create parameters, inputs and handles.
                        PostCook( true );

                        // Create default preset buffer.
                        CreateDefaultPreset();
#if WITH_EDITOR
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
                    HOUDINI_LOG_ERROR( TEXT( "    %s FinishedInstantiationWithErrors." ), *GetOwner()->GetName() );

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
        if ( HasBeenInstantiatedButNotCooked() || bParametersChanged || bComponentTransformHasChanged || bManualRecookRequested )
        {
            // Grab current time for delayed notification.
            HapiNotificationStarted = FPlatformTime::Seconds();

            // We just submitted a task, we want to continue ticking.
            bStopTicking = false;

            if ( bWaitingForUpstreamAssetsToInstantiate )
            {
                // We are waiting for upstream assets to instantiate.

                bWaitingForUpstreamAssetsToInstantiate = false;
                for ( auto LocalInput : Inputs )
                    bWaitingForUpstreamAssetsToInstantiate |= LocalInput->DoesInputAssetNeedInstantiation();

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
                UpdateLoadedInputs();

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
                bComponentTransformHasChanged = false;

                // Create asset cooking task object and submit it for processing.
                StartTaskAssetCooking();
            }
            else if ( bEnableCooking || bComponentTransformHasChanged || bManualRecookRequested )
            {
                // Uploads parameters and cooks the asset if cook on parameter
                // changed or cook on transform changed is enabled

                // Upload changed parameters back to HAPI.
                UploadChangedParameters();

                // Reset tranform changed flag.
                bComponentTransformHasChanged = false;

                // Create asset cooking task object and submit it for processing.
                StartTaskAssetCooking();
            }
            else if (bParametersChanged && !bEnableCooking)
            {
                // Cooking is disabled, but we still need to upload the parameters
                // and update the editor properties
                UploadChangedParameters();

                // Update properties panel.
                UpdateEditorProperties(true);
            }
            else 
            {
                // This will only happen if cooking is disabled.
                bStopTicking = true;
            }
        }
        else
        {
            // Nothing has changed, we can terminate ticking.
            bStopTicking = true;
        }
    }

    if (bNeedToUpdateNavigationSystem)
    {
#ifdef WITH_EDITOR
        // We need to update the navigation system manually with the Actor or the NavMesh will not update properly
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World && World->GetNavigationSystem())
        {
            AHoudiniAssetActor* HoudiniActor = GetHoudiniAssetActorOwner();
            if(HoudiniActor)
                World->GetNavigationSystem()->UpdateActorAndComponentsInNavOctree(*HoudiniActor);
        }
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

    if( !HoudiniAssetActor )
        return;

    FPropertyEditorModule & PropertyModule =
        FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

    // Locate the details panel.
    FName DetailsPanelName = "LevelEditorSelectionDetails";
    TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

    if ( DetailsView.IsValid() )
    {
        if ( DetailsView->IsLocked() )
        {
            // If details panel is locked, locate selected actors and check if this component belongs to one of them.

            const TArray< TWeakObjectPtr< AActor > > & SelectedDetailActors = DetailsView->GetSelectedActors();
            bool bFoundActor = false;

            for ( int32 ActorIdx = 0, ActorNum = SelectedDetailActors.Num(); ActorIdx < ActorNum; ++ActorIdx )
            {
                TWeakObjectPtr< AActor > SelectedActor = SelectedDetailActors[ ActorIdx ];
                if ( SelectedActor.IsValid() && SelectedActor.Get() == HoudiniAssetActor )
                {
                    bFoundActor = true;
                    break;
                }
            }

            // Details panel is locked, but our actor is not selected.
            if ( !bFoundActor )
                return;
        }
        else
        {
            // If our actor is not selected (and details panel is not locked) don't do any updates.
            if ( !HoudiniAssetActor->IsSelected() )
                return;
        }
    }
    else
    {
        // We have no details panel, nothing to update.
        return;
    }

    if ( GEditor && HoudiniAssetActor && bIsNativeComponent )
    {
        if ( bConditionalUpdate && FSlateApplication::Get().HasAnyMouseCaptor() )
        {
            // We want to avoid UI update if this is a conditional update and widget has captured the mouse.
            StartHoudiniUIUpdateTicking();
            return;
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

void
UHoudiniAssetComponent::StartTaskAssetInstantiation( bool bLocalLoadedComponent, bool bStartTicking )
{
    // We do not want to be instantiated twice
    bAssetIsBeingInstantiated = true;

    // We first need to make sure all our asset inputs have been instantiated and reconnected.
    for ( auto LocalInput : Inputs )
    {
        bool bInputAssetNeedsInstantiation = LocalInput->DoesInputAssetNeedInstantiation();
        if ( bInputAssetNeedsInstantiation )
        {
            UHoudiniAssetComponent * LocalInputAssetComponent = LocalInput->GetConnectedInputAssetComponent();
            LocalInputAssetComponent->NotifyParameterChanged( nullptr );
            bWaitingForUpstreamAssetsToInstantiate = true;
        }
    }

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
        if ( FHoudiniEngineUtils::IsValidAssetId( GetAssetId() ) )
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
        if ( FHoudiniEngineUtils::IsValidAssetId( GetAssetId() ) )
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
        bool bInstantiate = false;

        if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
        {
            if ( FHoudiniEngineUtils::GetAssetPreset( AssetId, PresetBuffer ) )
            {
                // We need to delete the asset and request a new one.
                StartTaskAssetDeletion();

                // We need to instantiate.
                bInstantiate = true;
            }
        }
        else
        {
            if ( bLoadedComponent )
            {
                // We need to instantiate.
                bInstantiate = true;
            }
        }

        if ( bInstantiate )
        {
            HapiGUID = FGuid::NewGuid();

            // If this is a loaded component, then we just need to instantiate.
            bLoadedComponentRequiresInstantiation = true;
            bParametersChanged = true;

            StartHoudiniTicking();
        }
    }
}

void
UHoudiniAssetComponent::StartTaskAssetDeletion()
{
    if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) && bIsNativeComponent )
    {
        // Generate GUID for our new task.
        FGuid HapiDeletionGUID = FGuid::NewGuid();

        // Create asset deletion task object and submit it for processing.
        FHoudiniEngineTask Task( EHoudiniEngineTaskType::AssetDeletion, HapiDeletionGUID );
        Task.AssetId = AssetId;
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
        // Generate GUID for our new task.
        HapiGUID = FGuid::NewGuid();

        FHoudiniEngineTask Task( EHoudiniEngineTaskType::AssetCooking, HapiGUID );
        Task.ActorName = GetOuter()->GetName();
        Task.AssetComponent = this;
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

    if ( !bIsNativeComponent )
        return;

    UProperty * Property = PropertyChangedEvent.MemberProperty;

    if ( !Property )
        return;

    if ( Property->GetName() == TEXT( "Mobility" ) )
    {
        // Mobility was changed, we need to update it for all attached components as well.
        const auto & LocalAttachChildren = GetAttachChildren();
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

    if ( Property->HasMetaData( TEXT( "Category" ) ) )
    {
        const FString & Category = Property->GetMetaData( TEXT( "Category" ) );
        static const FString CategoryHoudiniGeneratedStaticMeshSettings = TEXT( "HoudiniGeneratedStaticMeshSettings" );
        static const FString CategoryLighting = TEXT( "Lighting" );

        if ( CategoryHoudiniGeneratedStaticMeshSettings == Category )
        {
            // We are changing one of the mesh generation properties, we need to update all static meshes.
            for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TIterator Iter( StaticMeshComponents ); Iter; ++Iter )
            {
                UStaticMesh * StaticMesh = Iter.Key();
		if ( !StaticMesh )
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
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, CastShadow );
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
            else if ( Property->GetName() == TEXT( "bLightAsIfStatic" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bLightAsIfStatic );
            }
            else if ( Property->GetName() == TEXT( "bLightAttachmentsAsGroup" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, bLightAttachmentsAsGroup );
            }
            else if ( Property->GetName() == TEXT( "IndirectLightingCacheQuality" ) )
            {
                HOUDINI_UPDATE_ALL_CHILD_COMPONENTS( UPrimitiveComponent, IndirectLightingCacheQuality );
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
    }

    // Mark this component as imported.
    bComponentCopyImported = true;
}

void
UHoudiniAssetComponent::OnAssetPostImport( UFactory * Factory, UObject * Object )
{
    if (!bComponentCopyImported || (CopiedHoudiniComponent == nullptr))
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
    if ( FHoudiniEngineUtils::IsValidAssetId( CopiedHoudiniComponentAssetId ) )
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
    for( TMap< FHoudiniGeoPartObject, UStaticMesh * >::TIterator Iter( CopiedHoudiniComponent->StaticMeshes );
        Iter; ++Iter )
    {
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        UStaticMesh * StaticMesh = Iter.Value();

        // Duplicate static mesh and all related generated Houdini materials and textures.
        UStaticMesh * DuplicatedStaticMesh =
            FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage( StaticMesh, this, HoudiniGeoPartObject, FHoudiniEngineUtils::GetStaticMeshesCookMode() );

        if( DuplicatedStaticMesh )
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
    for( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * >::TIterator
        Iter( CopiedHoudiniComponent->SplineComponents ); Iter; ++Iter )
    {
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
            
        // Duplicate spline component.
        UHoudiniSplineComponent * DuplicatedSplineComponent = 
            DuplicateObject< UHoudiniSplineComponent >( HoudiniSplineComponent, this );

        if ( DuplicatedSplineComponent )
        {
            DuplicatedSplineComponent->SetFlags( RF_Transactional | RF_Public );
            SplineComponents.Add( HoudiniGeoPartObject, DuplicatedSplineComponent );
        }
    }

    // We need to duplicate landscapes.
    for ( TMap< FHoudiniGeoPartObject, ALandscape * >::TIterator
        Iter(CopiedHoudiniComponent->LandscapeComponents); Iter; ++Iter)
    {
        FHoudiniGeoPartObject & HoudiniGeoPartObject = Iter.Key();
        ALandscape * HoudiniLandscape = Iter.Value();

        // Duplicate spline component.
        ALandscape * DuplicatedLandscape =
            DuplicateObject< ALandscape >( HoudiniLandscape, this );

        if ( DuplicatedLandscape )
        {
            DuplicatedLandscape->SetFlags(RF_Transactional | RF_Public);
            LandscapeComponents.Add(HoudiniGeoPartObject, DuplicatedLandscape);
        }
    }

    // Perform any necessary post loading.
    PostLoad();

    DuplicateHandles( CopiedHoudiniComponent );

    // Mark this component as no longer copy imported and reset copied component.
    bComponentCopyImported = false;
    CopiedHoudiniComponent = nullptr;
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

        if ( !StaticMeshComponent || !StaticMesh )
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
        if ( InstanceInput )
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
                    UInstancedStaticMeshComponent * InstancedStaticMeshComponent =
                        InstancedStaticMeshComponents[ Idx ];

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
    // Only if asset has been cooked.
    if ( AssetCookCount > 0 )
    {
        // If we have to upload transforms.
        if ( bUploadTransformsToHoudiniEngine )
        {
            // Retrieve the current component-to-world transform for this component.
            if ( !FHoudiniEngineUtils::HapiSetAssetTransform( AssetId, GetComponentTransform() ) )
                HOUDINI_LOG_MESSAGE( TEXT( "Failed Uploading Transformation change back to HAPI." ) );
        }

        // If transforms trigger cooks, we need to schedule one.
        if ( bTransformChangeTriggersCooks )
        {
            bComponentTransformHasChanged = true;
            StartHoudiniTicking();
        }
    }
}

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
    if ( TempCookFolder.IsEmpty() )
    {
        // Get runtime settings.
        const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
        check( HoudiniRuntimeSettings );

        return HoudiniRuntimeSettings->TemporaryCookFolder;
    }
    return TempCookFolder;
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

    if( HoudiniAsset )
    {
        return FString::Printf( TEXT("%s_%d_%d_%d_%d"), *GetOwner()->GetName(),
            GeoPartObject.ObjectId, GeoPartObject.GeoId, GeoPartObject.PartId, GeoPartObject.SplitId);
    }

    return FString();
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

FBoxSphereBounds
UHoudiniAssetComponent::CalcBounds( const FTransform & LocalToWorld ) const
{
    FBoxSphereBounds LocalBounds;

    const auto & LocalAttachedChildren = GetAttachChildren();

    if ( LocalAttachedChildren.Num() == 0 )
        LocalBounds = FBoxSphereBounds( FBox(
            -FVector( 1.0f, 1.0f, 1.0f ) * HALF_WORLD_MAX,
            FVector( 1.0f, 1.0f, 1.0f ) * HALF_WORLD_MAX ) );
    else if ( LocalAttachedChildren[ 0 ] )
        LocalBounds = LocalAttachedChildren[ 0 ]->CalcBounds( LocalToWorld );

    for ( int32 Idx = 1; Idx < LocalAttachedChildren.Num(); ++Idx )
    {
        if ( LocalAttachedChildren[ Idx ] )
            LocalBounds = LocalBounds + LocalAttachedChildren[ Idx ]->CalcBounds( LocalToWorld );
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
    const auto & LocalAttachChildren = GetAttachChildren();
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
        if ( StaticMeshComponent )
            StaticMeshComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    }

    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * >::TIterator Iter( SplineComponents ); Iter; ++Iter )
    {
        UHoudiniSplineComponent * HoudiniSplineComponent = Iter.Value();
        if( HoudiniSplineComponent )
            HoudiniSplineComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    }

    for ( TMap< FString, UHoudiniHandleComponent * >::TIterator Iter( HandleComponents ); Iter; ++Iter )
    {
        UHoudiniHandleComponent * HoudiniHandleComponent = Iter.Value();
        if ( HoudiniHandleComponent )
            HoudiniHandleComponent->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    }

    for (TMap< FHoudiniGeoPartObject, ALandscape * >::TIterator Iter(LandscapeComponents); Iter; ++Iter)
    {
        ALandscape * HoudiniLandscape = Iter.Value();
        if ( HoudiniLandscape )
            HoudiniLandscape->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
    }
}

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

#if WITH_EDITOR

    // Subscribe to delegates.
    SubscribeEditorDelegates();

#endif
}

void
UHoudiniAssetComponent::BeginDestroy()
{
    Super::BeginDestroy();
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

#if WITH_EDITOR

    // Release all Houdini related resources.
    ResetHoudiniResources();

    // Unsubscribe from Editor events.
    UnsubscribeEditorDelegates();

#endif

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
            if ( StaticMeshComponent )
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
             // Recreate render state.
            InstanceInput->RecreateRenderStates();

            // Recreate physics state.
            InstanceInput->RecreatePhysicsStates();
        }
    }
}

void
UHoudiniAssetComponent::OnUnregister()
{
    Super::OnUnregister();
}

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
    StaticMeshMap.Add( HoudiniGeoPartObject, FHoudiniEngine::Get().GetHoudiniLogoStaticMesh() );
    CreateObjectGeoPartResources( StaticMeshMap );
    bContainsHoudiniLogoGeometry = true;
}

void
UHoudiniAssetComponent::PostLoad()
{
    Super::PostLoad();

#if WITH_EDITOR
    // Only do PostLoad stuff if we are in the editor world
    if(UWorld* World = GetWorld())
        if ( World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::Inactive )
            return;

    SanitizePostLoad();
#endif

    // We loaded a component which has no asset associated with it.
    if ( !HoudiniAsset )
    {
        // Set geometry to be Houdini logo geometry, since we have no other geometry.
        CreateStaticMeshHoudiniLogoResource( StaticMeshes );
        return;
    }

#if WITH_EDITOR

    // Show busy cursor.
    FScopedBusyCursor ScopedBusyCursor;

#endif

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

    // Update static mobility.
    if ( Mobility == EComponentMobility::Static )
    {
        const auto & LocalAttachChildren = GetAttachChildren();
        for ( TArray< USceneComponent * >::TConstIterator Iter( LocalAttachChildren ); Iter; ++Iter )
        {
            USceneComponent * SceneComponent = *Iter;
            SceneComponent->SetMobility( EComponentMobility::Static );
        }
    }

    // Need to update rendering information.
    UpdateRenderingInformation();

#if WITH_EDITOR

    // Force editor to redraw viewports.
    if ( GEditor )
        GEditor->RedrawAllViewports();

    // Update properties panel after instantiation.
    UpdateEditorProperties( false );

#endif
}

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
        if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
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
    uint32 HoudiniAssetComponentVersion = GetLinkerCustomVersion( FHoudiniCustomSerializationVersion::GUID );
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
            if ( FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
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
                if ( HoudiniAssetParameter )
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

    if (HoudiniAssetComponentVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_COOK_TEMP_PACKAGES)
    {
        TMap<FString, FString> SavedPackages;
        if ( Ar.IsSaving() )
        {
            for (TMap<FString, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryPackages); IterPackage; ++IterPackage)
            {
                UPackage * Package = IterPackage.Value().Get();

                FString sValue;
                if ( Package )
                    sValue = Package->GetFName().ToString();

                FString sKey = IterPackage.Key();
                SavedPackages.Add(sKey, sValue);
            }
        }

        Ar << SavedPackages;

        if ( Ar.IsLoading() )
        {
            for (TMap<FString, FString > ::TIterator IterPackage(SavedPackages); IterPackage; ++IterPackage)
            {
                FString sKey = IterPackage.Key();
                FString PackageFile = IterPackage.Value();

                UPackage * Package = nullptr;
                if ( !PackageFile.IsEmpty() )
                    Package = LoadPackage(nullptr, *PackageFile, LOAD_None);

                CookedTemporaryPackages.Add( sKey, Package );
            }
        }
    }

    if ( Ar.IsLoading() && bIsNativeComponent )
    {
        // This component has been loaded.
        bLoadedComponent = true;
    }
}

#if WITH_EDITOR

void
UHoudiniAssetComponent::SetStaticMeshGenerationParameters( UStaticMesh * StaticMesh ) const
{
    if ( !StaticMesh )
        return;

    // Make sure static mesh has a new lighting guid.
    StaticMesh->LightingGuid = FGuid::NewGuid();
    StaticMesh->LODGroup = NAME_None;

    // Set resolution of lightmap.
    StaticMesh->LightMapResolution = GeneratedLightMapResolution;

    // Set Bias multiplier for Light Propagation Volume lighting.
    StaticMesh->LpvBiasMultiplier = GeneratedLpvBiasMultiplier;

    // Set light map coordinate index.
    StaticMesh->LightMapCoordinateIndex = GeneratedLightMapCoordinateIndex;

    // Set method for LOD texture factor computation.
    /* TODO_414
    //StaticMesh->bUseMaximumStreamingTexelRatio = bGeneratedUseMaximumStreamingTexelRatio;

    // Set distance where textures using UV 0 are streamed in/out.  - GOES ON COMPONENT
    // StaticMesh->StreamingDistanceMultiplier = GeneratedStreamingDistanceMultiplier;
    */

    // Add user data.
    for ( int32 AssetUserDataIdx = 0; AssetUserDataIdx < GeneratedAssetUserData.Num(); ++AssetUserDataIdx )
        StaticMesh->AddAssetUserData( GeneratedAssetUserData[ AssetUserDataIdx ] );

    StaticMesh->CreateBodySetup();
    UBodySetup * BodySetup = StaticMesh->BodySetup;
    check( BodySetup );

    // Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
    BodySetup->bDoubleSidedGeometry = bGeneratedDoubleSidedGeometry;

    // Assign physical material for simple collision.
    BodySetup->PhysMaterial = GeneratedPhysMaterial;

    // Assign collision trace behavior.
    BodySetup->CollisionTraceFlag = GeneratedCollisionTraceFlag;

    // Assign walkable slope behavior.
    BodySetup->WalkableSlopeOverride = GeneratedWalkableSlopeOverride;

    // We want to use all of geometry for collision detection purposes.
    BodySetup->bMeshCollideAll = true;
}

AActor *
UHoudiniAssetComponent::CloneComponentsAndCreateActor()
{
    // Display busy cursor.
    FScopedBusyCursor ScopedBusyCursor;

    ULevel * Level = GetHoudiniAssetActorOwner()->GetLevel();
    AActor * Actor = NewObject< AActor >( Level, NAME_None );
    Actor->AddToRoot();

    USceneComponent * RootComponent =
        NewObject< USceneComponent >( Actor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional );

    RootComponent->Mobility = EComponentMobility::Movable;
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

            // Retrieve referenced static mesh component.
            UStaticMeshComponent * StaticMeshComponent = LocateStaticMeshComponent( StaticMesh );

            if ( !StaticMeshComponent )
                continue;

            // Bake the referenced static mesh.
            UStaticMesh * OutStaticMesh = FHoudiniEngineUtils::DuplicateStaticMeshAndCreatePackage(
                StaticMesh, this, HoudiniGeoPartObject, FHoudiniEngineUtils::EBakeMode::CreateNewAssets );

            if ( OutStaticMesh )
                FAssetRegistryModule::AssetCreated( OutStaticMesh );

            // Create static mesh component for baked mesh.
            UStaticMeshComponent * DuplicatedComponent =
                NewObject< UStaticMeshComponent >( Actor, UStaticMeshComponent::StaticClass(), NAME_None );//, RF_Transactional );

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

            DuplicatedComponent->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepRelativeTransform );
            DuplicatedComponent->RegisterComponent();
        }
    }

    // Duplicate instanced static mesh components.
    {
        for( auto& InstanceInput : InstanceInputs )
        {
            if ( InstanceInput )
                InstanceInput->CloneComponentsAndAttachToActor( Actor );
        }
    }

    return Actor;
}

bool
UHoudiniAssetComponent::IsCookingEnabled() const
{
    return bEnableCooking;
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
        UPackage * Package = IterPackage.Value().Get();
        if (Package)
        {
            FString PackageName = Package->GetName();
            if ( !PackageName.IsEmpty() && ( PackageName != TEXT( "None" ) ) )
                continue;
        }

        bCookedContentNeedRecook = true;
        break;
    }

    // Check the cooked materials refer to something..
    for ( TMap< FHoudiniGeoPartObject, TWeakObjectPtr< UPackage > > ::TIterator IterPackage( CookedTemporaryStaticMeshPackages ); IterPackage; ++IterPackage )
    {
        UPackage * Package = IterPackage.Value().Get();
        if ( Package )
        {
            FString PackageName = Package->GetName();
            if ( !PackageName.IsEmpty() && ( PackageName != TEXT("None") ) )
                continue;
        }

        bCookedContentNeedRecook = true;
        break;
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
        if(nullptr == Iter.Value)
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
        if(nullptr == Input)
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
        if( SM && SMC )
        {
            auto& StaticMeshMaterials = SM->StaticMaterials;
            for( int32 MaterialIdx = 0; MaterialIdx < StaticMeshMaterials.Num(); ++MaterialIdx )
            {
                if( nullptr == StaticMeshMaterials[ MaterialIdx ].MaterialInterface )
                {
                    auto DefaultMI = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();
                    StaticMeshMaterials[ MaterialIdx ].MaterialInterface = DefaultMI;
                    SMC->SetMaterial( MaterialIdx, DefaultMI );
                }
            }
        }
    }
}

#endif

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

            if ( StaticMesh && ObjectName.Len() > 0 )
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
        UStaticMesh * StaticMesh = Iter.Value();

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

    TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * > NewSplineComponents;
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

bool
UHoudiniAssetComponent::CreateParameters()
{
    if ( !FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        // There's no Houdini asset, we can return.
        return true;
    }

    bool bTreatRampParametersAsMultiparms = false;
    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings )
        bTreatRampParametersAsMultiparms = HoudiniRuntimeSettings->bTreatRampParametersAsMultiparms;

    HAPI_Result Result = HAPI_RESULT_SUCCESS;

    // Map of newly created and reused parameters.
    TMap< HAPI_ParmId, UHoudiniAssetParameter * > NewParameters;
    TMap< FString, UHoudiniAssetParameter * > NewParameterByName;

    HAPI_AssetInfo AssetInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAssetInfo(
        FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ), false );

    HAPI_NodeInfo NodeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
        FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &NodeInfo ), false );


    if ( NodeInfo.parmCount > 0 )
    {
        // Retrieve parameters.
        TArray< HAPI_ParmInfo > ParmInfos;
        ParmInfos.SetNumUninitialized( NodeInfo.parmCount );
        HOUDINI_CHECK_ERROR_RETURN(
            FHoudiniApi::GetParameters(
                FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &ParmInfos[ 0 ], 0,
                NodeInfo.parmCount ), false );

        // Create properties for parameters.
        for ( int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx )
        {
            // Retrieve param info at this index.
            const HAPI_ParmInfo & ParmInfo = ParmInfos[ ParamIdx ];

            // If the parameter is corrupt, skip it
            if( ParmInfo.id < 0 || ParmInfo.childIndex < 0 )
            {
                HOUDINI_LOG_WARNING( TEXT( "Corrupt parameter %d detected, skipping.  Note: Plug-in does not support nested Multiparm parameters" ), ParamIdx );
                continue;
            }

            // If parameter is invisible, skip it.
            if ( ParmInfo.invisible )
                continue;

            // Check if any parent folder of this parameter is invisible 
            bool SkipParm = false;
            HAPI_ParmId ParentId = ParmInfo.parentId;
            while (ParentId > 0 && !SkipParm)
            {
                if( const HAPI_ParmInfo* ParentInfoPtr = ParmInfos.FindByPredicate( [=]( const HAPI_ParmInfo& Info ) {
                    return Info.id == ParentId;
                } ) )
                {
                    if( ParentInfoPtr->invisible && ParentInfoPtr->type == HAPI_PARMTYPE_FOLDER )
                        SkipParm = true;
                    ParentId = ParentInfoPtr->parentId;
                }
                else
                {
                    HOUDINI_LOG_ERROR( TEXT("Could not find parent of parameter %d"), ParmInfo.id );
                    SkipParm = true;
                }
            }

            if ( SkipParm )
                continue;

            UHoudiniAssetParameter * HoudiniAssetParameter = nullptr;

            {
                // See if this parameter has already been created.
                // We can't use HAPI_ParmId because that is not unique to parameter instances, so instead
                // we find the existing parameter by name
                FString NewParmName;
                FHoudiniEngineString( ParmInfo.nameSH ).ToFString( NewParmName );
                UHoudiniAssetParameter * const * FoundHoudiniAssetParameter = ParameterByName.Find( NewParmName );

                // If parameter exists, we can reuse it.
                if( FoundHoudiniAssetParameter && *FoundHoudiniAssetParameter )
                {
                    // sanity check that type and tuple size hasn't changed
                    if( ( *FoundHoudiniAssetParameter )->GetTupleSize() == ParmInfo.size )
                    {
                        UClass* FoundClass = ( *FoundHoudiniAssetParameter )->GetClass();
                        bool FailedTypeCheck = true;
                        switch( ParmInfo.type )
                        {
                            case HAPI_PARMTYPE_STRING:
                                if ( !ParmInfo.choiceCount )
                                    FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniAssetParameterString >();
                                else
                                    FailedTypeCheck &= !FoundClass->IsChildOf< UHoudiniAssetParameterChoice >();
                                break;
                            case HAPI_PARMTYPE_INT:
                                if( !ParmInfo.choiceCount )
                                    FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterInt>();
                                else
                                    FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterChoice>();
                                break;
                            case HAPI_PARMTYPE_FLOAT:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFloat>();
                                break;
                            case HAPI_PARMTYPE_TOGGLE:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterToggle>();
                                break;
                            case HAPI_PARMTYPE_COLOR:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterColor>();
                                break;
                            case HAPI_PARMTYPE_LABEL:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterLabel>();
                                break;
                            case HAPI_PARMTYPE_BUTTON:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterButton>();
                                break;
                            case HAPI_PARMTYPE_SEPARATOR:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterSeparator>();
                                break;
                            case HAPI_PARMTYPE_FOLDERLIST:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFolderList>();
                                break;
                            case HAPI_PARMTYPE_FOLDER:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFolder>();
                                break;
                            case HAPI_PARMTYPE_MULTIPARMLIST:
                                if( !bTreatRampParametersAsMultiparms && ( HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType ||
                                    HAPI_RAMPTYPE_COLOR == ParmInfo.rampType ) )
                                {
                                    FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterRamp>();
                                }
                                else
                                    FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterMultiparm>();
                                break;
                            case HAPI_PARMTYPE_PATH_FILE:
                            case HAPI_PARMTYPE_PATH_FILE_GEO:
                            case HAPI_PARMTYPE_PATH_FILE_IMAGE:
                                FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterFile>();
                                break;
                            case HAPI_PARMTYPE_NODE:
                                if( ParmInfo.inputNodeType == HAPI_NODETYPE_ANY ||
                                    ParmInfo.inputNodeType == HAPI_NODETYPE_SOP ||
                                    ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ )
                                {
                                    FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetInput>();
                                }
                                else
                                {
                                    FailedTypeCheck &= !FoundClass->IsChildOf<UHoudiniAssetParameterString>();
                                }
                                break;
                        }

                        if ( !FailedTypeCheck )
                        {
                            HoudiniAssetParameter = *FoundHoudiniAssetParameter;

                            // Remove parameter from current map.
                            Parameters.Remove( ParmInfo.id );

                            // Reinitialize parameter and add it to map.
                            HoudiniAssetParameter->CreateParameter( this, nullptr, AssetInfo.nodeId, ParmInfo );
                            NewParameters.Add( ParmInfo.id, HoudiniAssetParameter );
                            continue;
                        }
                    }
                }
            }

            // If we ever get here, it means we are creating new parameters so we'll need a full UI update
            bEditorPropertiesNeedFullUpdate = true;

            switch ( ParmInfo.type )
            {
                case HAPI_PARMTYPE_STRING:
                {
                    if ( !ParmInfo.choiceCount )
                        HoudiniAssetParameter = UHoudiniAssetParameterString::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo );
                    else
                        HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo );

                    break;
                }

                case HAPI_PARMTYPE_INT:
                {
                    if ( !ParmInfo.choiceCount )
                        HoudiniAssetParameter = UHoudiniAssetParameterInt::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo );
                    else
                        HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo );

                    break;
                }

                case HAPI_PARMTYPE_FLOAT:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFloat::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_TOGGLE:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_COLOR:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterColor::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_LABEL:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterLabel::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_BUTTON:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterButton::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_SEPARATOR:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterSeparator::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_FOLDERLIST:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFolderList::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo);
                    break;
                }

                case HAPI_PARMTYPE_FOLDER:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFolder::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo);
                    break;
                }

                case HAPI_PARMTYPE_MULTIPARMLIST:
                {
                    if ( !bTreatRampParametersAsMultiparms && ( HAPI_RAMPTYPE_FLOAT == ParmInfo.rampType ||
                        HAPI_RAMPTYPE_COLOR == ParmInfo.rampType ) )
                    {
                        HoudiniAssetParameter = UHoudiniAssetParameterRamp::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo );
                    }
                    else
                    {
                        HoudiniAssetParameter = UHoudiniAssetParameterMultiparm::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo );
                    }

                    break;
                }

                case HAPI_PARMTYPE_PATH_FILE:
                case HAPI_PARMTYPE_PATH_FILE_GEO:
                case HAPI_PARMTYPE_PATH_FILE_IMAGE:
                {
                    HoudiniAssetParameter = UHoudiniAssetParameterFile::Create(
                        this, nullptr, AssetInfo.nodeId, ParmInfo );
                    break;
                }

                case HAPI_PARMTYPE_NODE:
                {
                    if (ParmInfo.inputNodeType == HAPI_NODETYPE_ANY || 
                        ParmInfo.inputNodeType == HAPI_NODETYPE_SOP || 
                        ParmInfo.inputNodeType == HAPI_NODETYPE_OBJ)
                    {
                        HoudiniAssetParameter = UHoudiniAssetInput::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo);
                    }
                    else
                    {
                        HoudiniAssetParameter = UHoudiniAssetParameterString::Create(
                            this, nullptr, AssetInfo.nodeId, ParmInfo);
                    }
                    break;
                }
                
                default:
                {
                    // Just ignore unsupported types for now.
                    HOUDINI_LOG_WARNING(TEXT("Parameter Type (%d) is unsupported"), static_cast<int32>(ParmInfo.type));
                    continue;
                }
            }

            // Add this parameter to the map.
            NewParameters.Add( ParmInfo.id, HoudiniAssetParameter );
        }

        // We do another pass to patch parent links.
        for ( int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx )
        {
            // Retrieve param info at this index.
            const HAPI_ParmInfo & ParmInfo = ParmInfos[ ParamIdx ];

            // Locate corresponding parameter.
            UHoudiniAssetParameter * const * FoundHoudiniAssetParameter = NewParameters.Find( ParmInfo.id );
            UHoudiniAssetParameter * HoudiniAssetParentParameter = nullptr;

            if ( FoundHoudiniAssetParameter )
            {
                UHoudiniAssetParameter * HoudiniAssetParameter = *FoundHoudiniAssetParameter;

                // Get parent parm id.
                HAPI_ParmId ParentParmId = HoudiniAssetParameter->GetParmParentId();
                if ( ParentParmId != -1 )
                {
                    // Locate corresponding parent parameter.
                    UHoudiniAssetParameter * const * FoundHoudiniAssetParentParameter = NewParameters.Find( ParentParmId );
                    if ( FoundHoudiniAssetParentParameter )
                        HoudiniAssetParentParameter = *FoundHoudiniAssetParentParameter;
                }

                // Set parent for this parameter.
                HoudiniAssetParameter->SetParentParameter( HoudiniAssetParentParameter );

                if ( ParmInfo.type == HAPI_PARMTYPE_FOLDERLIST )
                {
                    // For folder lists we need to add children manually.
                    HoudiniAssetParameter->ResetChildParameters();

                    for ( int32 ChildIdx = 0; ChildIdx < ParmInfo.size; ++ChildIdx )
                    {
                        // Children folder parm infos come after folder list parm info.
                        const HAPI_ParmInfo & ChildParmInfo = ParmInfos[ ParamIdx + ChildIdx + 1 ];

                        UHoudiniAssetParameter * const * FoundHoudiniAssetParameterChild =
                            NewParameters.Find( ChildParmInfo.id );

                        if ( FoundHoudiniAssetParameterChild )
                        {
                            UHoudiniAssetParameter * HoudiniAssetParameterChild = *FoundHoudiniAssetParameterChild;
                            HoudiniAssetParameterChild->SetParentParameter( HoudiniAssetParameter );
                        }
                    }
                }
            }
        }

        // Another pass to notify parameters that all children parameters have been assigned and to populate name look
        // up map for faster querying.
        for ( int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx )
        {
            // Retrieve param info at this index.
            const HAPI_ParmInfo& ParmInfo = ParmInfos[ ParamIdx ];

            // Locate corresponding parameter.
            UHoudiniAssetParameter * const * FoundHoudiniAssetParameter = NewParameters.Find( ParmInfo.id );
            UHoudiniAssetParameter * HoudiniAssetParentParameter = nullptr;

            if ( FoundHoudiniAssetParameter )
            {
                UHoudiniAssetParameter * HoudiniAssetParameter = *FoundHoudiniAssetParameter;
                if ( HoudiniAssetParameter->HasChildParameters() )
                    HoudiniAssetParameter->NotifyChildParametersCreated();

                // Add this parameter to parameter name look up map.
                NewParameterByName.Add( HoudiniAssetParameter->GetParameterName(), HoudiniAssetParameter );
            }
        }
    }

    // Remove all unused parameters.
    ClearParameters();

    // Update parameters.
    Parameters = NewParameters;
    ParameterByName = NewParameterByName;

    return true;
}

void
UHoudiniAssetComponent::NotifyParameterChanged( UHoudiniAssetParameter * HoudiniAssetParameter )
{
    if ( bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId( AssetId ) && !bAssetIsBeingInstantiated )
        bLoadedComponentRequiresInstantiation = true;

    if ( HoudiniAssetParameter )
    {
        // Some parameter types won't require a full update of the editor panel
        // This will avoid breaking the current selection
        UClass* FoundClass = HoudiniAssetParameter->GetClass();
        if ( FoundClass->IsChildOf< UHoudiniAssetParameterFloat >()
            || FoundClass->IsChildOf< UHoudiniAssetParameterInt >()
            || FoundClass->IsChildOf< UHoudiniAssetParameterString >() )
            bEditorPropertiesNeedFullUpdate = false;
    }

    bParametersChanged = true;
    StartHoudiniTicking();
}

void
UHoudiniAssetComponent::NotifyHoudiniSplineChanged( UHoudiniSplineComponent * HoudiniSplineComponent )
{
    if ( bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId( AssetId ) && !bAssetIsBeingInstantiated )
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

            // If parameter has changed, upload it to HAPI.
            if ( HoudiniAssetParameter->HasChanged() )
            {
                Success &= HoudiniAssetParameter->UploadParameterValue();
            }
        }
    }

    if( !Success )
    {
        HOUDINI_LOG_ERROR(TEXT("%s UploadChangedParameters failed"), *GetOwner()->GetName());
    }

    // We no longer have changed parameters.
    bParametersChanged = false;
}

void
UHoudiniAssetComponent::UpdateLoadedParameters()
{
    HAPI_AssetInfo AssetInfo;
    if ( FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ) != HAPI_RESULT_SUCCESS )
        return;

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        HoudiniAssetParameter->SetNodeId( AssetInfo.nodeId );
    }
}

bool
UHoudiniAssetComponent::CreateHandles()
{
    if ( !FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        // There's no Houdini asset, we can return.
        return false;
    }

    HAPI_AssetInfo AssetInfo;
    if ( FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ) != HAPI_RESULT_SUCCESS )
        return false;

    FHandleComponentMap NewHandleComponents;

    // If we have handles.
    if ( AssetInfo.handleCount > 0 )
    {
        TArray< HAPI_HandleInfo > HandleInfos;
        HandleInfos.SetNumZeroed( AssetInfo.handleCount );

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
                HOUDINI_LOG_DISPLAY( TEXT( "%s: Unsupported Handle Type %s for handle %s" ), *GetOwner()->GetName(), *TypeName, *HandleName );
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
    if ( !FHoudiniEngineUtils::IsValidAssetId( AssetId ) )
    {
        // There's no Houdini asset, we can return.
        return;
    }

    // Inputs have been created already.
    if ( Inputs.Num() > 0 )
        return;

    HAPI_AssetInfo AssetInfo;
    int32 InputCount = 0;
    if ( FHoudiniApi::GetAssetInfo( FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo ) == HAPI_RESULT_SUCCESS
        && AssetInfo.hasEverCooked )
    {
        InputCount = AssetInfo.geoInputCount;
    }

    // Create inputs.
    Inputs.SetNumZeroed( InputCount );
    for ( int32 InputIdx = 0; InputIdx < InputCount; ++InputIdx )
        Inputs[ InputIdx ] = UHoudiniAssetInput::Create( this, InputIdx );
}

void
UHoudiniAssetComponent::UpdateLoadedInputs()
{
    bool Success = true;
    for ( TArray< UHoudiniAssetInput * >::TIterator IterInputs( Inputs ); IterInputs; ++IterInputs )
    {
        UHoudiniAssetInput * HoudiniAssetInput = *IterInputs;
        if (!HoudiniAssetInput)
            continue;

        Success &= HoudiniAssetInput->ChangeInputType(HoudiniAssetInput->GetChoiceIndex());
        Success &= HoudiniAssetInput->UploadParameterValue();
    }

    if( !Success )
    {
        HOUDINI_LOG_ERROR(TEXT("%s UpdateLoadedInputs failed"), *GetOwner()->GetName());
    }
}

bool
UHoudiniAssetComponent::RefreshEditableNodesAfterLoad()
{
    // For some reason, we need to go through all the editable nodes once
    // To "Activate" them...
    HAPI_AssetInfo AssetInfo;
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

            HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
                FHoudiniEngine::Get().GetSession(),
                EditableNodeIds[nEditable],
                &CurrentEditableGeoInfo), false);

            if ( CurrentEditableGeoInfo.type != HAPI_GEOTYPE_CURVE )
                continue;

            FString NodePathTemp;
            if ( !FHoudiniEngineUtils::HapiGetNodePath( CurrentEditableGeoInfo.nodeId, AssetId, NodePathTemp ) )
                continue;

            // We need to refresh the spline corresponding to that node
            for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * >::TIterator Iter( SplineComponents ); Iter; ++Iter )
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
    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * >::TIterator Iter( SplineComponents ); Iter; ++Iter )
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
        if ( InstanceInput->GetGeoPartObject().GetNodePath() == GeoPart.GetNodePath() )
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

            if ( UHoudiniAssetInstanceInput * FoundHoudiniAssetInstanceInput = LocateInstanceInput( GeoPart ) )
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

            if ( !HoudiniAssetInstanceInput )
            {
                // Invalid instance input.
                HOUDINI_LOG_WARNING( TEXT( "Inavlid Instance Input" ) );
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
    TMap< HAPI_ParmId, UHoudiniAssetParameter * > & InParameters = DuplicatedHoudiniComponent->Parameters;
    TMap< FString, UHoudiniAssetParameter * > & InParametersByName = DuplicatedHoudiniComponent->ParameterByName;

    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        HAPI_ParmId HoudiniAssetParameterKey = IterParams.Key();
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();

        if (HoudiniAssetParameterKey == -1)
            continue;

        // Duplicate parameter.
        UHoudiniAssetParameter * DuplicatedHoudiniAssetParameter = HoudiniAssetParameter->Duplicate( DuplicatedHoudiniComponent );

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
    for ( auto const & SrcNameToHandle : SrcAssetComponent->HandleComponents )
    {
        // Duplicate spline component.
        UHoudiniHandleComponent * NewHandleComponent =
            DuplicateObject< UHoudiniHandleComponent >( SrcNameToHandle.Value, this );

        if ( NewHandleComponent )
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
    TArray< UHoudiniAssetInput * > & InInputs = DuplicatedHoudiniComponent->Inputs;

    for ( int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx )
    {
        // Retrieve input at this index.
        UHoudiniAssetInput * AssetInput = Inputs[ InputIdx ];
        if (AssetInput == nullptr)
            continue;

        // Duplicate input.
        UHoudiniAssetInput * DuplicateAssetInput = DuplicateObject( AssetInput, DuplicatedHoudiniComponent );
        if (DuplicateAssetInput == nullptr)
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
    auto& InInstanceInputs = DuplicatedHoudiniComponent->InstanceInputs;

    for ( auto& HoudiniAssetInstanceInput : InstanceInputs )
    {
        UHoudiniAssetInstanceInput * DuplicatedHoudiniAssetInstanceInput =
            UHoudiniAssetInstanceInput::Create( DuplicatedHoudiniComponent, HoudiniAssetInstanceInput );

        // PIE does not like standalone flags.
        DuplicatedHoudiniAssetInstanceInput->ClearFlags( RF_Standalone );

        InInstanceInputs.Add( DuplicatedHoudiniAssetInstanceInput );

        // remap our instanced objects (only necessary for unbaked assets)
        for( UHoudiniAssetInstanceInputField* InputField : DuplicatedHoudiniAssetInstanceInput->GetInstanceInputFields() )
        {
            InputField->FixInstancedObjects( ReplacementMap );
        }
    }
}

bool
UHoudiniAssetComponent::CreateAllLandscapes(const TArray< FHoudiniGeoPartObject > & FoundVolumes)
{
    // First, we need to extract proper height data from FoundVolumes
    TArray< const FHoudiniGeoPartObject* > FoundHeightfields;
    for ( TArray< FHoudiniGeoPartObject >::TConstIterator Iter(FoundVolumes); Iter; ++Iter )
    {
        const FHoudiniGeoPartObject & HoudiniGeoPartObject = *Iter;

        // Retrieve node id from geo part.
        HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(AssetId);
        if ( NodeId == -1 )
            continue;

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo CurrentVolumeInfo;
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            NodeId, HoudiniGeoPartObject.PartId,
            &CurrentVolumeInfo ), false );

        // We're only interested in heightfields
        FString CurrentVolumeName;
        FHoudiniEngineString( CurrentVolumeInfo.nameSH ).ToFString( CurrentVolumeName );
        if ( !CurrentVolumeName.Contains("height") )
            continue;

        // We're only handling single values for now
        if ( CurrentVolumeInfo.tupleSize != 1 )
            continue;

        // Terrains always have a ZSize of 1.
        if ( CurrentVolumeInfo.zLength != 1 )
            continue;

        // Values should be float
        if ( CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
            continue;

        FoundHeightfields.Add( &HoudiniGeoPartObject );
    }

    // Try to create a Landscape for each HeightData found
    TMap< FHoudiniGeoPartObject, ALandscape * > NewLandscapes;
    for ( TArray< const FHoudiniGeoPartObject* >::TConstIterator IterHeighfields(FoundHeightfields); IterHeighfields; ++IterHeighfields )
    {
        // Get the current Heightfield GeoPartObject
        const FHoudiniGeoPartObject* CurrentHeightfield = *IterHeighfields;
        if ( !CurrentHeightfield )
            continue;

        // Look for all the layers/masks corresponding to the current heightfield
        TArray< const FHoudiniGeoPartObject* > FoundLayers;
        HAPI_NodeId HeightFieldNodeId = CurrentHeightfield->HapiGeoGetNodeId(AssetId);
        for (TArray< FHoudiniGeoPartObject >::TConstIterator IterLayers(FoundVolumes); IterLayers; ++IterLayers)
        {
            const FHoudiniGeoPartObject & HoudiniGeoPartObject = *IterLayers;

            // Retrieve node id from geo part.
            HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId(AssetId);
            if ( NodeId == -1 )
                continue;

            if ( NodeId != HeightFieldNodeId )
                continue;

            // Retrieve the VolumeInfo
            HAPI_VolumeInfo CurrentVolumeInfo;
            HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetVolumeInfo(
                FHoudiniEngine::Get().GetSession(),
                NodeId, HoudiniGeoPartObject.PartId,
                &CurrentVolumeInfo ), false );

            // We're interesting in anything but height data
            FString CurrentVolumeName;
            FHoudiniEngineString( CurrentVolumeInfo.nameSH ).ToFString( CurrentVolumeName );
            if ( CurrentVolumeName.Contains( "height" ) )
                continue;

            // We're only handling single values for now
            if ( CurrentVolumeInfo.tupleSize != 1 )
                continue;

            // Terrains always have a ZSize of 1.
            if ( CurrentVolumeInfo.zLength != 1 )
                continue;

            // Values should be float
            if ( CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
                continue;

            FoundLayers.Add( &HoudiniGeoPartObject );
        }

        // We need to see if the current heightfield as an unreal_material or unreal_hole_material assigned to it
        UMaterialInterface* LandscapeMaterial = nullptr;
        UMaterialInterface* LandscapeHoleMaterial = nullptr;
        {
            std::string MarshallingAttributeNameMaterial = HAPI_UNREAL_ATTRIB_MATERIAL;
            std::string MarshallingAttributeNameMaterialHole = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;

            // Get runtime settings.
            const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
            if ( HoudiniRuntimeSettings )
            {
                if ( !HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty() )
                    FHoudiniEngineUtils::ConvertUnrealString(
                        HoudiniRuntimeSettings->MarshallingAttributeMaterial,
                        MarshallingAttributeNameMaterial );

                if ( !HoudiniRuntimeSettings->MarshallingAttributeMaterialHole.IsEmpty() )
                    FHoudiniEngineUtils::ConvertUnrealString(
                        HoudiniRuntimeSettings->MarshallingAttributeMaterialHole,
                        MarshallingAttributeNameMaterialHole );
            }

            TArray< FString > Materials;
            HAPI_AttributeInfo AttribMaterials;
            FMemory::Memset< HAPI_AttributeInfo >( AttribMaterials, 0 );

            // First, look for landscape material
            {
                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                    *CurrentHeightfield, MarshallingAttributeNameMaterial.c_str(),
                    AttribMaterials, Materials );

                if ( AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL )
                {
                    HOUDINI_LOG_WARNING( TEXT( "Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute." ) );
                    AttribMaterials.exists = false;
                    Materials.Empty();
                }

                if ( AttribMaterials.exists && Materials.Num() > 0 )
                {
                    // Load the material
                    LandscapeMaterial = Cast< UMaterialInterface >( StaticLoadObject(
                            UMaterialInterface::StaticClass(),
                            nullptr, *( Materials[ 0 ] ), nullptr, LOAD_NoWarn, nullptr ) );

                    if ( LandscapeMaterial )
                    {
                        // Make sure this material is in the assignemets before replacing it.
                        if ( !GetAssignmentMaterial( LandscapeMaterial->GetName() ) && HoudiniAssetComponentMaterials )
                            HoudiniAssetComponentMaterials->Assignments.Add( LandscapeMaterial->GetName(), LandscapeMaterial );

                        // See if we have a replacement material for this.
                        UMaterialInterface * ReplacementMaterialInterface = GetReplacementMaterial( *CurrentHeightfield, LandscapeMaterial->GetName());
                        if ( ReplacementMaterialInterface )
                            LandscapeMaterial = ReplacementMaterialInterface;
                    }
                }
            }

            Materials.Empty();
            FMemory::Memset< HAPI_AttributeInfo >(AttribMaterials, 0);

            // Then, for the hole_material
            {
                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                    *CurrentHeightfield, MarshallingAttributeNameMaterialHole.c_str(),
                    AttribMaterials, Materials );

                if ( AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL )
                {
                    HOUDINI_LOG_WARNING(TEXT( "Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute." ) );
                    AttribMaterials.exists = false;
                    Materials.Empty();
                }

                if ( AttribMaterials.exists && Materials.Num() > 0 )
                {
                    // Load the material
                    LandscapeHoleMaterial = Cast< UMaterialInterface >( StaticLoadObject(
                        UMaterialInterface::StaticClass(),
                        nullptr, *(Materials[0]), nullptr, LOAD_NoWarn, nullptr ) );

                    if ( LandscapeHoleMaterial )
                    {
                        // Make sure this material is in the assignemets before replacing it.
                        if ( !GetAssignmentMaterial( LandscapeHoleMaterial->GetName() ) && HoudiniAssetComponentMaterials )
                            HoudiniAssetComponentMaterials->Assignments.Add( LandscapeHoleMaterial->GetName(), LandscapeHoleMaterial );

                        // See if we have a replacement material for this.
                        UMaterialInterface * ReplacementMaterialInterface = GetReplacementMaterial( *CurrentHeightfield, LandscapeHoleMaterial->GetName() );
                        if ( ReplacementMaterialInterface )
                            LandscapeHoleMaterial = ReplacementMaterialInterface;
                    }
                }
            }
        }

        // Try to see if we can find materials used by the previous landscape for this Heightfield
        if ( LandscapeComponents.Contains( *CurrentHeightfield ) )
        {
            ALandscape* PreviousLandscape = LandscapeComponents[ *CurrentHeightfield ];
            if ( PreviousLandscape )
            {
                UMaterialInterface* PreviousLandscapeMaterial = PreviousLandscape->GetLandscapeMaterial();
                if ( PreviousLandscapeMaterial && PreviousLandscapeMaterial != LandscapeMaterial )
                {
                    ReplaceMaterial( *CurrentHeightfield, PreviousLandscapeMaterial, LandscapeMaterial, 0);
                    LandscapeMaterial = PreviousLandscapeMaterial;
                }

                UMaterialInterface* PreviousLandscapeHoleMaterial = PreviousLandscape->GetLandscapeHoleMaterial();
                if (PreviousLandscapeHoleMaterial && PreviousLandscapeHoleMaterial != LandscapeHoleMaterial)
                {
                    ReplaceMaterial(*CurrentHeightfield, PreviousLandscapeHoleMaterial, LandscapeHoleMaterial, 0);
                    LandscapeMaterial = PreviousLandscapeHoleMaterial;
                }
            }
        }

        // We can now create a Landscape for the heighfield
        // All found masks will be added as layers
        CreateLandscape( CurrentHeightfield, FoundLayers, NewLandscapes, LandscapeMaterial, LandscapeHoleMaterial );
    }

    // Replace the old landscapes with the new ones
    ClearLandscapes();
    LandscapeComponents = NewLandscapes;

    return true;
}


bool
UHoudiniAssetComponent::CreateLandscape(
    const FHoudiniGeoPartObject* HeightField,
    const TArray< const FHoudiniGeoPartObject* >& FoundLayers,
    TMap< FHoudiniGeoPartObject, ALandscape * >& NewLandscapes,
    UMaterialInterface* LandscapeMaterial,
    UMaterialInterface* LandscapeHoleMaterial )
{
    if ( !HeightField )
        return false;

    if ( !GEditor )
        return false;

    FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
    UWorld* MyWorld = EditorWorldContext.World();

    if ( !MyWorld )
        return false;

    const FHoudiniGeoPartObject HoudiniGeoPartObject = *HeightField;

    // Retrieve node id from geo part.
    HAPI_NodeId NodeId = HoudiniGeoPartObject.HapiGeoGetNodeId( AssetId );
    if ( NodeId == -1 )
        return false;

    // Retrieve the VolumeInfo
    HAPI_VolumeInfo CurrentVolumeInfo;
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetVolumeInfo(
        FHoudiniEngine::Get().GetSession(),
        NodeId, HoudiniGeoPartObject.PartId,
        &CurrentVolumeInfo ), false);

    // Make sure that the VolumeInfo corresponds to a Heightfield
    FString CurrentVolumeName;
    FHoudiniEngineString( CurrentVolumeInfo.nameSH ).ToFString( CurrentVolumeName );
    if ( !CurrentVolumeName.Contains( "height" ) )
        return false;

    // We're only handling single values for now
    if ( CurrentVolumeInfo.tupleSize != 1 )
        return false;

    // Terrains always have a ZSize of 1.
    if ( CurrentVolumeInfo.zLength != 1 )
        return false;

    // Values should be float
    if ( CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
        return false;

    int32 XSize = CurrentVolumeInfo.xLength;
    int32 YSize = CurrentVolumeInfo.yLength;
    int32 SizeInPoints = XSize * YSize;
    int32 TotalSize = SizeInPoints * CurrentVolumeInfo.tupleSize;

    if ( ( XSize < 2 ) || ( YSize < 2 ) )
        return false;

    int32 XOffset = CurrentVolumeInfo.minX;
    int32 YOffset = CurrentVolumeInfo.minY;

    FTransform CurrentVolumeTransform;
    FHoudiniEngineUtils::TranslateHapiTransform( CurrentVolumeInfo.transform, CurrentVolumeTransform );

    //--------------------------------------------------------------------------------------------------
    // 1. Reading and converting the Height values from HAPI
    //--------------------------------------------------------------------------------------------------    
    TArray<float> FloatHeightData;
    FloatHeightData.SetNumUninitialized( TotalSize );

    // Get all the heightfield data
    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetHeightFieldData(
        FHoudiniEngine::Get().GetSession(),
        NodeId, HoudiniGeoPartObject.PartId,
        FloatHeightData.GetData(),
        0, SizeInPoints ), false );

    // We will need the min and max value for the conversion to uint16
    float FloatMin = FloatHeightData[0];
    float FloatMax = FloatMin;
    for ( int32 n = 0; n < SizeInPoints; n++ )
    {
        if ( FloatHeightData[n] > FloatMax )
            FloatMax = FloatHeightData[n];
        else if ( FloatHeightData[n] < FloatMin )
            FloatMin = FloatHeightData[n];
    }

    // We don't want to create a flat landscape
    /*
    if (FloatMin == FloatMax)
    return false;
    */

    //--------------------------------------------------------------------------------------------------
    // 2. Convert values to uint16 using doubles to get the maximum precision during the conversion
    //--------------------------------------------------------------------------------------------------
    TArray<uint16> IntHeightData;
    IntHeightData.SetNumUninitialized(SizeInPoints);

    // The ZRange in Houdini (in m)
    double ZRange = (double)( FloatMax - FloatMin );

    // The corresponding unreal digit range (as unreal uses uint16, max is 65535)
    double DigitRange = 49152.0;

    const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
    if ( HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseFullResolution )
        DigitRange = 65535.0;

    // The Offset in Digit used to center the values so that the terrain can be edited up/down
    double ZCenterOffset = FMath::FloorToDouble( ( 65535.0 - DigitRange ) / 2.0 );
    // The factor used to convert from Houdini's ZRange to the desired digit range
    double ZSpacing = ( ZRange != 0.0 ) ? ( DigitRange / ZRange ) : 0.0;

    int32 nUnrealIndex = 0;
    for ( int32 nY = 0; nY < YSize; nY++ )
    {
        for ( int32 nX = 0; nX < XSize; nX++ )
        {
            // Inverts the values from Houdini
            int32 nHoudini = ( XSize - 1 - nX ) + ( ( nY ) * XSize );
            if ( ImportAxis == HRSAI_Houdini )
                nHoudini = nUnrealIndex;

            // Get the double values in [0 - ZRange]
            double DoubleValue = (double)FloatHeightData[nHoudini] - (double)FloatMin;

            // Then convert it to [0 - DesiredRange] and center them 
            DoubleValue = DoubleValue * ZSpacing + ZCenterOffset;

            //dValue = FMath::Clamp(dValue, 0.0, 65535.0);
            IntHeightData[nUnrealIndex++] = FMath::RoundToInt( DoubleValue );
        }
    }

    // Calculating the final size, number of component, sections...
    // UE expects special values for these, so we might need to pad/resample the height data
    int32 OldXSize = XSize;
    int32 OldYSize = YSize;
    int32 NumSectionPerLandscapeComponent = 1;
    int32 NumQuadsPerLandscapeSection = XSize - 1;
    if ( !FHoudiniEngineUtils::ResizeHeightDataForLandscape(
            IntHeightData, XSize, YSize,
            NumSectionPerLandscapeComponent,
            NumQuadsPerLandscapeSection ) )
        return false;

    // We need to create the landscape now and assign it a new GUID so we can create the LayerInfos
    ALandscape* Landscape = MyWorld->SpawnActor<ALandscape>();
    if (!Landscape)
        return false;

    FGuid currentGUID = FGuid::NewGuid();
    Landscape->SetLandscapeGuid(currentGUID);

    // Notify the user if we heightfield data was resized
    if ( ( OldXSize != XSize ) && ( OldYSize != YSize ) )
    {
        HOUDINI_LOG_MESSAGE(
            TEXT("Creating Landscape: %s was resized from ( %d x %d ) to ( %d x %d )."),
            *( Landscape->GetName() ), OldXSize, OldYSize, XSize, YSize );
    }

    //--------------------------------------------------------------------------------------------------
    // 3. Calculating the proper transform for the landscape to be sized properly
    //--------------------------------------------------------------------------------------------------

    // Calculating the equivalent scale to match Houdini's Terrain Size in Unreal
    // For some reason, XY scale from Houdini needs to be multiplied by 2 to get the proper grid spacing ...
    FVector LandscapeScale;
    LandscapeScale.X = GeneratedGeometryScaleFactor * CurrentVolumeTransform.GetScale3D().X * 2.0f;
    LandscapeScale.Y = GeneratedGeometryScaleFactor * CurrentVolumeTransform.GetScale3D().Y * 2.0f;

    // Calculating the Z Scale so that the Z values in Unreal are the same as in Houdini
    // Unreal has a default Z range is 512m for a scale of a 100%
    LandscapeScale.Z = (float) ( (double)GeneratedGeometryScaleFactor * ( 65535.0 / DigitRange ) * ZRange / 512.0 );
    Landscape->SetActorRelativeScale3D( LandscapeScale );

    // Landscape rotation
    FRotator LandscapeRotation( 0.0, -90.0, 0.0 );
    Landscape->SetActorRelativeRotation( LandscapeRotation );

    // We'll use the position from Houdini, but we will need to offset the Z Position to center the 
    // values properly as the data has been offset by the conversion to uint16
    FVector LandscapePosition = CurrentVolumeTransform.GetLocation();

    // Trying to compensate the landscape grid orientation (XZ / YZ / XY)
    if ( FMath::Abs(LandscapePosition.Z) < SMALL_NUMBER )
    {
        // XZ orientation, we just need to invert Y
        LandscapePosition.Y = -LandscapePosition.Y;
        LandscapePosition.Z = 0.0f;
    }
    else if ( FMath::Abs( LandscapePosition.X ) < SMALL_NUMBER )
    {
        // YZ orientation
        LandscapePosition.X = LandscapePosition.Y;
        LandscapePosition.Y = -LandscapePosition.Z;
        LandscapePosition.Z = 0.0f;
    }
    else if ( FMath::Abs( LandscapePosition.Y ) < SMALL_NUMBER )
    {
        // XY orientation
        LandscapePosition.Y = -LandscapePosition.X;
        LandscapePosition.X = LandscapePosition.Z;
        LandscapePosition.Z = 0.0f;
    }

    // We need to calculate the position offset so that Houdini and Unreal have the same Zero position
    // In Unreal, zero has a height value of 32768.
    // These values are then divided by 128 internally, and then multiplied by the Landscape's Z scale
    // ( DIGIT - 32768 ) / 128 * ZScale = ZOffset
    
    // We need the Digit (Unreal) value of Houdini's zero for the scale calculation
    uint16 HoudiniZeroValueInDigit = FMath::RoundToInt( -(double)FloatMin * ZSpacing + ZCenterOffset );

    float ZOffset = -( (float)HoudiniZeroValueInDigit - 32768.0f ) / 128.0f * LandscapeScale.Z;
    LandscapePosition.Z = ZOffset;

    // We can now set the Landscape position
    Landscape->SetActorRelativeLocation( LandscapePosition );

    //--------------------------------------------------------------------------------------------------
    // 4. Reading and creating all layers
    //-------------------------------------------------------------------------------------------------- 
    // Try to create all the layers
    TArray<FLandscapeImportLayerInfo> ImportLayerInfos;
    ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

    TArray<UPackage *> CreatedLayerInfoPackage;
    for (TArray<const FHoudiniGeoPartObject *>::TConstIterator IterLayers( FoundLayers ); IterLayers; ++IterLayers)
    {
        HAPI_Result Result = HAPI_RESULT_SUCCESS;

        const FHoudiniGeoPartObject * LayerGeoPartObject = *IterLayers;
        if ( !LayerGeoPartObject )
            continue;

        // Retrieve node id from geo part.
        HAPI_NodeId LayerNodeId = LayerGeoPartObject->HapiGeoGetNodeId( AssetId );
        if ( LayerNodeId == -1 )
            continue;

        // Retrieve the VolumeInfo
        HAPI_VolumeInfo LayerVolumeInfo;
        HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetVolumeInfo(
            FHoudiniEngine::Get().GetSession(),
            LayerNodeId, LayerGeoPartObject->PartId,
            &LayerVolumeInfo ) );

        if ( Result != HAPI_RESULT_SUCCESS )
            continue;

        // We're only handling single values for now
        if ( LayerVolumeInfo.tupleSize != 1 )
            continue;

        // Terrains always have a ZSize of 1.
        if ( LayerVolumeInfo.zLength != 1 )
            continue;

        // .. and values should be float
        if ( LayerVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT )
            continue;

        int32 LayerXSize = LayerVolumeInfo.xLength;
        int32 LayerYSize = LayerVolumeInfo.yLength;
        int32 LayerSizeInPoints = LayerXSize * LayerYSize;

        // Read all the float data from HAPI
        TArray<float> FloatLayerData;
        FloatLayerData.SetNumUninitialized( LayerSizeInPoints );

        HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetHeightFieldData(
            FHoudiniEngine::Get().GetSession(),
            LayerNodeId, LayerGeoPartObject->PartId,
            FloatLayerData.GetData(),
            0, LayerSizeInPoints ) );

        if ( Result != HAPI_RESULT_SUCCESS )
            continue;

        // We will need the min and max value for the conversion to uint8
        float LayerMin = FloatLayerData[0];
        float LayerMax = LayerMin;
        for ( int32 n = 0; n < LayerSizeInPoints; n++ )
        {
            if ( FloatLayerData[n] > LayerMax )
                LayerMax = FloatLayerData[n];
            else if ( FloatLayerData[n] < LayerMin )
                LayerMin = FloatLayerData[n];
        }

        // No need to create flat layers as Unreal will remove them afterwards..
        if ( LayerMin == LayerMax )
            continue;

        // Creating the ImportLayerInfo and LayerInfo objects
        FString LayerString;
        FHoudiniEngineString( LayerVolumeInfo.nameSH ).ToFString( LayerString );
        //LayerString = LayerString.Replace( TEXT( "." ), TEXT( "" ) );
        ObjectTools::SanitizeObjectName( LayerString );

        FName LayerName( *LayerString );
        FLandscapeImportLayerInfo currentLayerInfo( LayerName );
        //currentLayerInfo.LayerInfo = Landscape->CreateLayerInfo( LayerString.GetCharArray().GetData() );
        UPackage * Package;
        currentLayerInfo.LayerInfo = CreateLandscapeLayerInfoObject( LayerString.GetCharArray().GetData(), Package );
        if ( !currentLayerInfo.LayerInfo )
            continue;

        CreatedLayerInfoPackage.Add( Package );

        // Convert the float data to uint8
        currentLayerInfo.LayerData.SetNumUninitialized( LayerSizeInPoints );

        // Calculating the factor used to convert from Houdini's ZRange to [0 255]
        double LayerZRange = ( LayerMax - LayerMin );
        double LayerZSpacing = ( LayerZRange != 0.0 ) ? (255.0 / (double)( LayerMax - LayerMin ) ) : 0.0;
        
        nUnrealIndex = 0;
        for ( int32 nY = 0; nY < LayerYSize; nY++ )
        {
            for ( int32 nX = 0; nX < LayerXSize; nX++ )
            {
                // Inverts the values from Houdini
                int32 nHoudini = ( LayerXSize - 1 - nX ) + ( ( nY ) * LayerXSize );
                if ( ImportAxis == HRSAI_Houdini )
                    nHoudini = nUnrealIndex;

                // Get the double values in [0 - ZRange]
                double DoubleValue = (double)FloatLayerData[nHoudini] - (double)LayerMin;

                // Then convert it to [0 - 255]
                DoubleValue *= LayerZSpacing;

                //dValue = FMath::Clamp(dValue, 0.0, 65535.0);
                currentLayerInfo.LayerData[nUnrealIndex++] = FMath::RoundToInt( DoubleValue );
            }
        }

        // Finally, we need to resize the data to fit with the new landscape size
        if (!FHoudiniEngineUtils::ResizeLayerDataForLandscape(
            currentLayerInfo.LayerData,
            LayerXSize, LayerYSize,
            XSize, YSize ) )
                continue;

        currentLayerInfo.LayerInfo->bNoWeightBlend = false;

        // Mark the package dirty...
        //Package->MarkPackageDirty();

        ImportLayerInfos.Add( currentLayerInfo );
    }

    // Save the packages created for the LayerInfos
    if ( CreatedLayerInfoPackage.Num() > 0 )
        FEditorFileUtils::PromptForCheckoutAndSave( CreatedLayerInfoPackage, true, false );

    //--------------------------------------------------------------------------------------------------
    // 5. Import the landscape data
    //--------------------------------------------------------------------------------------------------

    // Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
    Landscape->bCastStaticShadow = false;

    if( LandscapeMaterial )
        Landscape->LandscapeMaterial = LandscapeMaterial;

    if ( LandscapeHoleMaterial )
        Landscape->LandscapeHoleMaterial = LandscapeHoleMaterial;

    // Import the data
    Landscape->Import(
        currentGUID,
        XOffset, YOffset, 
        XOffset + XSize - 1, YOffset + YSize - 1,
        NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
        &( IntHeightData[0] ), NULL, 
        ImportLayerInfos, ImportLayerType );

    // FROM UE:
    // automatically calculate a lighting LOD that won't crash lightmass (hopefully)
    // < 2048x2048 -> LOD0,  >=2048x2048 -> LOD1,  >= 4096x4096 -> LOD2,  >= 8192x8192 -> LOD3
    Landscape->StaticLightingLOD = FMath::DivideAndRoundUp( FMath::CeilLogTwo( ( XSize * YSize ) / ( 2048 * 2048 ) + 1 ), (uint32) 2 );
    
    Landscape->AttachToComponent( this, FAttachmentTransformRules::KeepRelativeTransform );
    Landscape->RegisterAllComponents();

    // Add the new landscape to the map
    NewLandscapes.Add( HoudiniGeoPartObject, Landscape );

    return true;
}

ULandscapeLayerInfoObject *
UHoudiniAssetComponent::CreateLandscapeLayerInfoObject( const TCHAR* LayerName, UPackage*& Package)
{
    //const FGuid & ComponentGUID = GetComponentGuid();
    FString ComponentGUIDString = GetComponentGuid().ToString().Left( FHoudiniEngineUtils::PackageGUIDComponentNameLength );

    // Create the LandscapeInfoObjectName from the Asset name and the mask name
    FName LayerObjectName = FName( *( HoudiniAsset->GetName() + ComponentGUIDString + FString::Printf( TEXT( "_LayerInfoObject_%s" ), LayerName ) ) );
    
    // Save the package in the temp folder
    FString Path = GetTempCookFolder().ToString() + TEXT( "/" );
    FString PackageName = Path + LayerObjectName.ToString();

    // See if package exists, if it does, reuse it
    //UPackage * Package = FindPackage( nullptr , *PackageName );
    Package = FindPackage( nullptr, *PackageName );
    if ( !Package )
    {
        // Package does not exsits, create it
        Package = CreatePackage( nullptr, *PackageName );
    }

    ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>( Package, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional );
    LayerInfo->LayerName = LayerName;

    // Notify the asset registry
    FAssetRegistryModule::AssetCreated( LayerInfo );

    // Mark the package dirty...
    Package->MarkPackageDirty();

    return LayerInfo;
}

#endif

void
UHoudiniAssetComponent::ClearInstanceInputs()
{
    for ( auto& InstanceInput : InstanceInputs )
    {
        InstanceInput->ConditionalBeginDestroy();
    }

    InstanceInputs.Empty();
}

void
UHoudiniAssetComponent::ClearCurves()
{
    for ( TMap< FHoudiniGeoPartObject, UHoudiniSplineComponent * >::TIterator Iter( SplineComponents ); Iter; ++Iter )
    {
        UHoudiniSplineComponent * SplineComponent = Iter.Value();

        SplineComponent->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
        SplineComponent->UnregisterComponent();
        SplineComponent->DestroyComponent();
    }

    SplineComponents.Empty();
}

void
UHoudiniAssetComponent::ClearLandscapes()
{
    for (TMap< FHoudiniGeoPartObject, ALandscape * >::TIterator Iter(LandscapeComponents); Iter; ++Iter)
    {
        ALandscape * HoudiniLandscape = Iter.Value();

        //HoudiniLandscape->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
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
        if ( Iter.Value )
        {
            Iter.Value->ConditionalBeginDestroy();
        }
        else
        {
            HOUDINI_LOG_WARNING(TEXT("%s: null parameter when clearing"), *GetOwner()->GetName());
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
        TSet< int32 > & LocalInputIndicies = IterAssets.Value();
        for ( auto LocalInputIndex : LocalInputIndicies )
        {
            DownstreamAsset->Inputs[ LocalInputIndex ]->ExternalDisconnectInputAssetActor();
        }
    }

    DownstreamAssetConnections.Empty();
}

void
UHoudiniAssetComponent::ClearCookTempFile()
{
    // First, Clean up the assignement/replacement map
    HoudiniAssetComponentMaterials->ResetMaterialInfo();

    // Then delete all the materials
    //for ( TMap<FString, UPackage* > ::TIterator IterPackage( CookedTemporaryPackages );
    for ( TMap<FString, TWeakObjectPtr< UPackage > > ::TIterator IterPackage(CookedTemporaryPackages );
        IterPackage; ++IterPackage)
    {
        UPackage * Package = IterPackage.Value().Get();
        if ( !Package )
            continue;

        Package->ClearFlags( RF_Standalone );
        Package->ConditionalBeginDestroy();
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
        Package->ConditionalBeginDestroy();
    }

    CookedTemporaryStaticMeshPackages.Empty();
}

UStaticMesh *
UHoudiniAssetComponent::LocateStaticMesh( const FHoudiniGeoPartObject & HoudiniGeoPartObject ) const
{
    UStaticMesh * const * FoundStaticMesh = StaticMeshes.Find( HoudiniGeoPartObject );
    UStaticMesh * StaticMesh = nullptr;

    if ( FoundStaticMesh )
        StaticMesh = *FoundStaticMesh;

    if ( StaticMesh && StaticMesh->IsPendingKill() )
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
    const UStaticMesh * StaticMesh,
    TArray< UInstancedStaticMeshComponent * > & Components ) const
{
    Components.Empty();

    bool bResult = false;

    for ( auto& InstanceInput : InstanceInputs )
    {
        if ( InstanceInput )
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

    if ( SplineComponent && SplineComponent->IsPendingKill() )
        return nullptr;

    return SplineComponent;
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
        if (!HoudiniAssetInput)
            continue;

        if (HoudiniAssetInput->GetFName() != NAME_None)
            continue;

        // Calling Rename with null parameters will make sure the instanced input has a unique name
        HoudiniAssetInput->Rename();
    }

    Ar << Inputs;

    if ( Ar.IsTransacting() )
    {
        for ( int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx )
            Inputs[ InputIdx ]->Serialize( Ar );
    }
    else if ( Ar.IsLoading() )
    {
        for ( int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx )
        {
            if ( !Inputs[ InputIdx ] )
                continue;

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
        
        int32 HoudiniAssetComponentVersion = GetLinkerCustomVersion( FHoudiniCustomSerializationVersion::GUID );
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
            if ( !HoudiniInstanceInput )
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
        if (!HoudiniAssetParameter)
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

        HoudiniAssetParameter->SetHoudiniAssetComponent( this );

        HAPI_ParmId ParentParameterId = HoudiniAssetParameter->GetParmParentId();
        if ( ParentParameterId != -1 )
        {
            UHoudiniAssetParameter * const * FoundParentParameter = Parameters.Find( ParentParameterId );
            if ( FoundParentParameter )
                HoudiniAssetParameter->SetParentParameter( *FoundParentParameter );
        }
    }

    // Notify all parameters that have children that loading is complete.
    for ( TMap< HAPI_ParmId, UHoudiniAssetParameter * >::TIterator IterParams( Parameters ); IterParams; ++IterParams )
    {
        UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
        if ( HoudiniAssetParameter )
        {
            if ( HoudiniAssetParameter->HasChildParameters() )
                HoudiniAssetParameter->NotifyChildParametersLoaded();
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
        if ( StaticMeshComponent )
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

    if ( HoudiniAssetComponentMaterials )
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
    if ( HoudiniAssetComponentMaterials )
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

    if ( HoudiniAssetComponentMaterials )
    {
        TMap< FString, UMaterialInterface * > & MaterialAssignments = HoudiniAssetComponentMaterials->Assignments;

        UMaterialInterface * const * FoundMaterial = MaterialAssignments.Find( MaterialName );
        if ( FoundMaterial )
            Material = *FoundMaterial;
    }

    return Material;
}

bool
UHoudiniAssetComponent::ReplaceMaterial(
    const FHoudiniGeoPartObject & HoudiniGeoPartObject,
    UMaterialInterface * NewMaterialInterface,
    UMaterialInterface * OldMaterialInterface,
    int32 MaterialIndex )
{
    if ( !HoudiniAssetComponentMaterials )
        return false;

    // Check that we do own this GeoPartObject, either via StaticMeshes or Landscapes
    UStaticMesh * StaticMesh = LocateStaticMesh( HoudiniGeoPartObject );
    if ( StaticMesh )
    {
        UStaticMeshComponent * StaticMeshComponent = LocateStaticMeshComponent( StaticMesh );
        if ( !StaticMeshComponent )
        {
            TArray< UInstancedStaticMeshComponent * > InstancedStaticMeshComponents;
            if (!LocateInstancedStaticMeshComponents( StaticMesh, InstancedStaticMeshComponents ) )
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

    UMaterialInterface * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial();

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
    if ( HoudiniAssetComponentMaterials )
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
UHoudiniAssetComponent::HasAnySockets() const
{
    // Return true if any of our StaticMeshComponent HasAnySocket
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent )
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
     //Query all the sockets in our StaticMeshComponents
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter(StaticMeshComponents); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent )
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
    //Query all the sockets in our StaticMeshComponents
    for ( TMap< UStaticMesh *, UStaticMeshComponent * >::TConstIterator Iter( StaticMeshComponents ); Iter; ++Iter )
    {
        UStaticMeshComponent * StaticMeshComponent = Iter.Value();
        if ( !StaticMeshComponent )
            continue;

        if ( StaticMeshComponent->DoesSocketExist( SocketName ) )
            return true;
    }
    
    return Super::DoesSocketExist( SocketName );
}


FTransform 
UHoudiniAssetComponent::GetSocketTransform( FName InSocketName, ERelativeTransformSpace TransformSpace ) const
{
    //Query all the sockets in our StaticMeshComponents
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

#undef LOCTEXT_NAMESPACE