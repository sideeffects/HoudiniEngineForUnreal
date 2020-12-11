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

#include "HoudiniAssetInput.h"

#include "HoudiniApi.h"
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
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Selection.h"
#include "Internationalization/Internationalization.h"
#include "EngineUtils.h" // for TActorIterator<>

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#endif

#if WITH_EDITOR
// Allows checking of objects currently being dragged around
struct FHoudiniMoveTracker
{
    FHoudiniMoveTracker() : IsObjectMoving(false)
    {
        GEditor->OnBeginObjectMovement().AddLambda([this](UObject&) { IsObjectMoving = true; });
        GEditor->OnEndObjectMovement().AddLambda([this](UObject&) { IsObjectMoving = false; });
    }
    static FHoudiniMoveTracker& Get() { static FHoudiniMoveTracker Instance; return Instance; }

    bool IsObjectMoving;
};
#endif

static FName NAME_HoudiniNoUpload( TEXT( "HoudiniNoUpload" ) );


#define LOCTEXT_NAMESPACE HOUDINI_MODULE_RUNTIME

void
FHoudiniAssetInputOutlinerMesh::Serialize( FArchive & Ar )
{
    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    HoudiniAssetParameterVersion = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
    Ar << HoudiniAssetParameterVersion;

    Ar << ActorPtr;
    if ( ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_ACTOR_PATHNAME )
        && ( HoudiniAssetParameterVersion != VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_419_SERIALIZATION_FIX ) )
    {
        Ar << ActorPathName;
    }

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

    // UE4.19 SERIALIZATION FIX:
    // The component materials serialization (24) was actually missing in the UE4.19 H17.0 / H16.5 plugin.
    // However subsequent serialized changes (25+) were present in those version. This caused crashes when loading
    // a level that was saved with 4.19+16.5/17.0 on a newer version of Unreal or Houdini...
    // If the serialized version is exactly that of the fix, we can ignore the materials paths as well
    if ( ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INPUT_SAVE_MAT )
        && (HoudiniAssetParameterVersion != VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_419_SERIALIZATION_FIX ) )
        Ar << MeshComponentsMaterials;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_OUTLINER_INSTANCE_INDEX )
        Ar << InstanceIndex;
}

void 
FHoudiniAssetInputOutlinerMesh::RebuildSplineTransformsArrayIfNeeded()
{
    // Rebuilding the SplineTransform array after reloading the asset
    // This is required to properly detect Transform changes after loading the asset.

    // We need an Unreal spline
    if ( !SplineComponent || SplineComponent->IsPendingKill() )
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
    if ( !SplineComponent || SplineComponent->IsPendingKill() )
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
    if ( StaticMeshComponent && !StaticMeshComponent->IsPendingKill() )
    {
        // Handle instances here
        UInstancedStaticMeshComponent * InstancedStaticMeshComponent = Cast< UInstancedStaticMeshComponent >( StaticMeshComponent );
        if (InstancedStaticMeshComponent )
        {
            FTransform InstanceTransform;
            if ( InstancedStaticMeshComponent->GetInstanceTransform( InstanceIndex, InstanceTransform, true ) )
                return !ComponentTransform.Equals( InstanceTransform );
        }
        else
            return !ComponentTransform.Equals( StaticMeshComponent->GetComponentTransform() );
    }

    if ( SplineComponent && !SplineComponent->IsPendingKill() )
        return !ComponentTransform.Equals( SplineComponent->GetComponentTransform() );

    return false;
}

bool
FHoudiniAssetInputOutlinerMesh::HasComponentMaterialsChanged() const
{
    if ( !StaticMeshComponent || StaticMeshComponent->IsPendingKill() )
        return false;

    if ( StaticMeshComponent->GetNumMaterials() != MeshComponentsMaterials.Num() )
        return true;

    for ( int32 n = 0; n < MeshComponentsMaterials.Num(); n++ )
    {
        UMaterialInterface* MI = StaticMeshComponent->GetMaterial(n);
        FString mat_interface_path = MI->GetPathName();
        if (!MI || MI->IsPendingKill())
            mat_interface_path = FString();

        if (mat_interface_path != MeshComponentsMaterials[ n ])
            return true;
    }

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

    if ( !StaticMesh && !StaticMeshComponent && !SplineComponent )
        return true;

    return false;
}


bool
FHoudiniAssetInputOutlinerMesh::TryToUpdateActorPtrFromActorPathName()
{
    // Ensure our current ActorPathName looks valid
    if (ActorPathName.IsEmpty() || ActorPathName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        return false;

    // We'll try to find the corresponding actor by browsing through all the actors in the world..
    // Get the editor world
    UWorld* World = nullptr;
#if WITH_EDITOR
    World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
#endif
    if (!World)
        return false;

    // Then try to find the actor corresponding to our path/name
    bool FoundActor = false;
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        if (ActorIt->GetPathName() != ActorPathName)
            continue;

        // We found the actor
        ActorPtr = *ActorIt;
        FoundActor = true;

        break;
    }

    if ( FoundActor )
    {
        // We need to invalid our components so they can be updated later
        // from the new actor
        StaticMesh = NULL;
        StaticMeshComponent = NULL;
        SplineComponent = NULL;
    }

    return FoundActor;
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
    bExportAllLODs = false;
    bExportSockets = false;
    bUpdateInputLandscape = false;

    ChoiceStringValue = TEXT( "" );

    // Initialize the arrays
    InputObjects.SetNumUninitialized( 1 );
    InputObjects[ 0 ] = nullptr;
    InputTransforms.SetNumUninitialized( 1 );
    InputTransforms[ 0 ] = FTransform::Identity;
    TransformUIExpanded.SetNumUninitialized( 1 );
    TransformUIExpanded[ 0 ] = false;
    SkeletonInputObjects.SetNumUninitialized(1);
    SkeletonInputObjects[0] = nullptr;
    InputLandscapeTransform = FTransform::Identity;
}

UHoudiniAssetInput *
UHoudiniAssetInput::Create( UObject * InPrimaryObject, int32 InInputIndex, HAPI_NodeId InNodeId )
{
    UHoudiniAssetInput * HoudiniAssetInput = nullptr;

    if (!InPrimaryObject || InPrimaryObject->IsPendingKill())
        return HoudiniAssetInput;

    // Get name of this input.
    HAPI_StringHandle InputStringHandle;
    if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInputName(
            FHoudiniEngine::Get().GetSession(),
            InNodeId, InInputIndex, &InputStringHandle ) )
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
    HAPI_NodeId InNodeId, 
    const HAPI_ParmInfo & ParmInfo)
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

    // Use the value of the object path param to preset the 
    // default object for Geometry inputs 
    HoudiniAssetInput->SetDefaultAssetFromHDA();

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
    {
        FString * ChoiceLabel = new FString(TEXT("Skeletal Mesh Input"));
        StringChoiceLabels.Add(TSharedPtr< FString >(ChoiceLabel));

        if (ChoiceIndex == EHoudiniAssetInputType::SkeletonInput)
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
            FHoudiniApi::SetParmStringValue(
                FHoudiniEngine::Get().GetSession(), NodeId, "",
                ParmId, 0 );
        }
        if ( InputAssetComponent )
            InputAssetComponent->RemoveDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );

        bInputAssetConnectedInHoudini = false;
        ConnectedAssetId = -1;
    }
    else
    {
        if ( bIsObjectPathParameter )
        {
            FHoudiniApi::SetParmStringValue(
                FHoudiniEngine::Get().GetSession(), NodeId, "", ParmId, 0 );
        }
        else
        {
            HAPI_NodeId HostAssetId = GetAssetId();
            if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) && FHoudiniEngineUtils::IsValidNodeId( HostAssetId ) )
                FHoudiniEngineUtils::HapiDisconnectAsset( HostAssetId, InputIndex );
        }

        // Destroy all the geo input assets
        for ( HAPI_NodeId AssetNodeId : CreatedInputDataAssetIds )
        {
            if ( FHoudiniEngineUtils::IsHoudiniNodeValid( AssetNodeId ) )
                FHoudiniEngineUtils::DestroyHoudiniAsset( AssetNodeId );
        }
        CreatedInputDataAssetIds.Empty();

        // Then simply destroy the input's parent OBJ node
        if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) )
        {
            HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId( ConnectedAssetId );
            if ( FHoudiniEngineUtils::IsHoudiniNodeValid( ParentId ) )
                FHoudiniEngineUtils::DestroyHoudiniAsset( ParentId );

            FHoudiniEngineUtils::DestroyHoudiniAsset( ConnectedAssetId );
        }
        ConnectedAssetId = -1;
        if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
        {
            // World Input Actors' Meshes need to have their corresponding Input Assets destroyed too.
            for ( int32 n = 0; n < InputOutlinerMeshArray.Num(); n++ )
            {
                InputOutlinerMeshArray[ n ].AssetId = -1;
            }
        }

        /*
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
        else if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
        {
            // Destroy the landscape node
            // If the landscape was sent as a heightfield, also destroy the heightfield's input node
            FHoudiniLandscapeUtils::DestroyLandscapeAssetNode( ConnectedAssetId, CreatedInputDataAssetIds );
        }
        else if ( ChoiceIndex == EHoudiniAssetInputType::SkeletonInput )
        {
            // Destroy all the skeleton input assets
            for ( HAPI_NodeId AssetNodeId : CreatedInputDataAssetIds )
            {
                FHoudiniEngineUtils::DestroyHoudiniAsset( AssetNodeId );
            }
            CreatedInputDataAssetIds.Empty();
        }

        if ( FHoudiniEngineUtils::IsValidAssetId( ConnectedAssetId ) )
        {
            FHoudiniEngineUtils::DestroyHoudiniAsset( ConnectedAssetId );
            ConnectedAssetId = -1;
        }
        */
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
UHoudiniAssetInput::PostEditUndo()
{
    Super::PostEditUndo();

    if ( InputCurve
        && !InputCurve->IsPendingKill()
        && ChoiceIndex == EHoudiniAssetInputType::CurveInput )
    {
        USceneComponent* RootComp = GetHoudiniAssetComponent();
        if ( RootComp && !RootComp->IsPendingKill() )
        {
            AActor* Owner = RootComp->GetOwner();
            if ( Owner && !Owner->IsPendingKill() )
                Owner->AddOwnedComponent( InputCurve );

            InputCurve->AttachToComponent( RootComp, FAttachmentTransformRules::KeepRelativeTransform );
            InputCurve->RegisterComponent();
            InputCurve->SetVisibility( true );
        }
    }
}

void
UHoudiniAssetInput::ForceSetInputObject(UObject * InObject, int32 AtIndex, bool CommitChange)
{
    EHoudiniAssetInputType::Enum NewInputType = EHoudiniAssetInputType::GeometryInput;
    if ( AActor* Actor = Cast<AActor>( InObject ) )
    {
        // Update the OutlinerInputArray from the actor
        UpdateInputOulinerArrayFromActor(Actor, true);
        if ( InputOutlinerMeshArray.Num() > 0 )
            NewInputType = EHoudiniAssetInputType::WorldInput;

        // Looking for houdini assets
        if ( AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>( Actor ) )
        {
            OnInputActorSelected( Actor );
            if ( InputAssetComponent )
                NewInputType = EHoudiniAssetInputType::AssetInput;

        }

        // Looking for Landscapes
        if (ALandscapeProxy * Landscape = Cast< ALandscapeProxy >( Actor ) )
        {
            // Store new landscape.
            OnLandscapeActorSelected( Actor );
            NewInputType = EHoudiniAssetInputType::LandscapeInput;
        }
    }
    else
    {
        // The object is not a world actor, so add it to the geo input
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
    }

    if( CommitChange )
    {
        // We must change ChoiceIndex's value manually before calling ChangeInputType: 
        // If we are forcing the input to an asset input, not doing so would actually destroy the 
        // parent HDA node on the houdini side, making the input HDA to need a reinstantiation on first cook...
        ChoiceIndex = NewInputType;

        ChangeInputType( NewInputType, true );
        MarkChanged();
    }
}

// Note: This method is only used for testing
void 
UHoudiniAssetInput::ClearInputs()
{
    ChangeInputType( EHoudiniAssetInputType::GeometryInput, true );
    InputOutlinerMeshArray.Empty();
    InputObjects.Empty();
    InputTransforms.Empty();
    TransformUIExpanded.Empty();
    MarkChanged();
}

#endif  // WITH_EDITOR

bool
UHoudiniAssetInput::ConnectInputNode()
{
    if (!FHoudiniEngineUtils::IsValidNodeId(ConnectedAssetId))
        return false;

    // Helper for connecting our input or setting the object path parameter
    if (!bIsObjectPathParameter)
    {
        // Now we can connect input node to the asset node.
        HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), GetAssetId(), InputIndex,
            ConnectedAssetId, 0), false);
    }
    else
    {
        if (!FHoudiniEngineUtils::IsValidNodeId(NodeId))
            return false;

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

    if ( !PrimaryObject || PrimaryObject->IsPendingKill() )
        return false;
    
    HAPI_NodeId HostAssetId = GetAssetId();

    switch ( ChoiceIndex )
    {
        case EHoudiniAssetInputType::GeometryInput:
        {
            if ( !InputObjects.Num() )
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
                    if ( !FHoudiniEngineUtils::HapiCreateInputNodeForObjects( 
                        HostAssetId, InputObjects, InputTransforms,
                        ConnectedAssetId, CreatedInputDataAssetIds,
                        false, bExportAllLODs, bExportSockets ) )
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
            if ( InputAssetComponent
                && !InputAssetComponent->IsPendingKill()
                && FHoudiniEngineUtils::IsValidNodeId( InputAssetComponent->GetAssetId() ) )
            {
                // Connect a new asset / we have previously connected asset 
                if (!bInputAssetConnectedInHoudini)
                    ConnectInputAssetActor();
                else
                    bChanged = false;

                Success &= bInputAssetConnectedInHoudini;
                Success &= UpdateObjectMergeTransformType();
            }
            else if ( bInputAssetConnectedInHoudini && ( !InputAssetComponent || InputAssetComponent->IsPendingKill() ) )
            {
                // Previously connected asset deleted/invalid
                DisconnectInputAssetActor();
            }
            else
            {
                // Nothing to connect
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
            if ( !FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) )
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
                    if ( !Parameter || Parameter->IsPendingKill() )
                        continue;

                    // We need to update the node id for the parameters.
                    Parameter->SetNodeId(ConnectedAssetId);

                    // Upload parameter value.
                    Success &= Parameter->UploadParameterValue();
                }
            }

            if ( ConnectedAssetId != -1 && InputCurve && !InputCurve->IsPendingKill() )
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
            
            if (bCreated && InputCurve && !InputCurve->IsPendingKill() )
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
            if ( !InputLandscapeProxy || InputLandscapeProxy->IsPendingKill() )
            {
                // Either landscape was reset or null landscape has been assigned.
                DisconnectAndDestroyInputAsset();
            }
            else
            {
                // Disconnect and destroy currently connected asset, if there's one.
                DisconnectAndDestroyInputAsset();

                // If we're auto-selecting the components, we need to get the asset's bound 
                FBox Bounds(ForceInitToZero);
                if ( bLandscapeAutoSelectComponent )
                {
                    // Get our asset's or our connected input asset's bounds
                    UHoudiniAssetComponent* AssetComponent = (UHoudiniAssetComponent*)PrimaryObject;
                    if (!AssetComponent || AssetComponent->IsPendingKill())
                        AssetComponent = InputAssetComponent;

                    // Get the bounds, without this input
                    if (AssetComponent && !AssetComponent->IsPendingKill())
                        Bounds = AssetComponent->GetAssetBounds(this, true);
                }

                // Connect input and create connected asset. Will return by reference.
                if ( !FHoudiniEngineUtils::HapiCreateInputNodeForLandscape(
                        HostAssetId, InputLandscapeProxy.Get(),
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
                    if ( !FHoudiniEngineUtils::HapiCreateInputNodeForWorldOutliner(
                        HostAssetId, InputOutlinerMeshArray, ConnectedAssetId, CreatedInputDataAssetIds,
                        UnrealSplineResolution, bExportAllLODs, bExportSockets ) )
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

        case EHoudiniAssetInputType::SkeletonInput:
        {
            if (!SkeletonInputObjects.Num())
            {
                // Either mesh was reset or null mesh has been assigned.
                DisconnectAndDestroyInputAsset();
            }
            else
            {
                if (bStaticMeshChanged || bLoadedParameter)
                {
                    // Disconnect and destroy currently connected asset, if there's one.
                    DisconnectAndDestroyInputAsset();

                    // Connect input and create connected asset. Will return by reference.
                    if ( !FHoudiniEngineUtils::HapiCreateInputNodeForObjects(
                        HostAssetId, SkeletonInputObjects, InputTransforms,
                        ConnectedAssetId, CreatedInputDataAssetIds, true ) )
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

UObject*
UHoudiniAssetInput::GetSkeletonInputObject( int32 AtIndex ) const
{
    return SkeletonInputObjects.IsValidIndex( AtIndex ) ? SkeletonInputObjects[ AtIndex ] : nullptr;
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
    if ( !PrimaryObject || PrimaryObject->IsPendingKill() )
        return false;

    uint32 nTransformType = -1;
    if ( bKeepWorldTransform == 2 )
        nTransformType = GetDefaultTranformTypeValue();
    else if ( bKeepWorldTransform )
        nTransformType = 1; 
    else
        nTransformType = 0;

    // Geometry inputs are always set to none
    if ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput )
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

    if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
    {
        // For World Inputs, we need to go through each asset selected
        // and change the transform type on the merge node's input
        for ( int32 n = 0; n < InputOutlinerMeshArray.Num(); n++ )
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
                bSuccess = false;
        }
    }

    return bSuccess;
}


bool
UHoudiniAssetInput::UpdateObjectMergePackBeforeMerge()
{
    if ( !PrimaryObject || PrimaryObject->IsPendingKill() )
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

    if (bIsObjectPathParameter)
	{
		// Directly change the Parameter xformtype
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
			FHoudiniEngine::Get().GetSession(),
			NodeId, sPack.c_str(), 0, nPackValue))
			bSuccess = true;
	}
	else
	{
		// Query the object merge's node ID via the input
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::QueryNodeInput(
			FHoudiniEngine::Get().GetSession(),
			HostAssetId, InputIndex, &InputNodeId))
		{
			// Change Parameter xformtype
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::SetParmIntValue(
				FHoudiniEngine::Get().GetSession(),
				InputNodeId, sPack.c_str(), 0, nPackValue))
				bSuccess = true;
		}
	}

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
  
    if ( InputCurve && !InputCurve->IsPendingKill() )
    {
        if (ChoiceIndex == EHoudiniAssetInputType::CurveInput)
        {
            // Set input callback object for this curve.
            InputCurve->SetHoudiniAssetInput(this);
            USceneComponent* RootComp = GetHoudiniAssetComponent();
            if( RootComp && !RootComp->IsPendingKill() )
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
        // As some of the Actors' components might not be properly initialized yet
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
    if (HoudiniAssetParameterVersion >= VER_HOUDINI_ENGINE_PARAM_LANDSCAPE_INPUT)
    {
        if (HoudiniAssetParameterVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_INPUT_SOFT_REF)
        {
            ALandscapeProxy* InputLandscapePtr = nullptr;
            Ar << InputLandscapePtr;

            InputLandscapeProxy = InputLandscapePtr;
        }
        else
        {
            Ar << InputLandscapeProxy;
        }

    }

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

    if ( ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_INPUT_LANDSCAPE_TRANSFORM )
        && ( HoudiniAssetParameterVersion != VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_419_SERIALIZATION_FIX ) )
        Ar << InputLandscapeTransform;
}

void
UHoudiniAssetInput::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetInput * HoudiniAssetInput = Cast< UHoudiniAssetInput >( InThis );
    if ( HoudiniAssetInput && !HoudiniAssetInput->IsPendingKill() )
    {
        // Add reference to held geometry object.
        if ( HoudiniAssetInput->InputObjects.Num() 
            && ( HoudiniAssetInput->GetChoiceIndex() == EHoudiniAssetInputType::GeometryInput ) )
            Collector.AddReferencedObjects( HoudiniAssetInput->InputObjects, InThis );

        /*
        // Add reference to held input asset component, if we have one.
        if ( HoudiniAssetInput->InputAssetComponent && !HoudiniAssetInput->InputAssetComponent->IsPendingKill() 
            && ( HoudiniAssetInput->GetChoiceIndex() == EHoudiniAssetInputType::AssetInput ) )
            Collector.AddReferencedObject( HoudiniAssetInput->InputAssetComponent, InThis );
        */

        // Add reference to held curve object.
        if ( HoudiniAssetInput->InputCurve && !HoudiniAssetInput->InputCurve->IsPendingKill() )
            Collector.AddReferencedObject( HoudiniAssetInput->InputCurve, InThis );

        // Do not add references to Actors/Landscapes in our world outliner as if they are in 
        // an other level it will prevent saving due to the external references...
        // Add references for all curve input parameters.
        for ( TMap< FString, UHoudiniAssetParameter * >::TIterator IterParams( HoudiniAssetInput->InputCurveParameters );
            IterParams; ++IterParams )
        {
            UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value();
            if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsPendingKill() )
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
    if ( InputCurve && !InputCurve->IsPendingKill() )
    {
        InputCurve->DetachFromComponent( FDetachmentTransformRules::KeepRelativeTransform );
        InputCurve->UnregisterComponent();
    }
}

void
UHoudiniAssetInput::DestroyInputCurve()
{
    // If we have spline, delete it.
    if ( InputCurve && !InputCurve->IsPendingKill() )
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

void
UHoudiniAssetInput::OnSkeletalMeshDropped( UObject * InObject, int32 AtIndex )
{
    UObject* InputObject = GetSkeletonInputObject( AtIndex );
    if ( InObject != InputObject )
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Skeleton Input Change" ),
            PrimaryObject );
        Modify();

        if ( SkeletonInputObjects.IsValidIndex( AtIndex ) )
        {
            SkeletonInputObjects[ AtIndex ] = InObject;
        }
        else
        {
            check( AtIndex == 0 );
            SkeletonInputObjects.Add( InObject );
        }

        /*
        if ( !InputTransforms.IsValidIndex( AtIndex ) )
            InputTransforms.Add( FTransform::Identity );

        if ( !TransformUIExpanded.IsValidIndex( AtIndex ) )
            TransformUIExpanded.Add( false );
        */

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

void UHoudiniAssetInput::OnStaticMeshBrowse( int32 AtIndex )
{
    UObject* InputObject = GetInputObject( AtIndex );
    if ( GEditor && InputObject )
    {
        TArray< UObject * > Objects;
        Objects.Add( InputObject );
        GEditor->SyncBrowserToObjects( Objects );
    }
}

void UHoudiniAssetInput::OnSkeletalMeshBrowse( int32 AtIndex )
{
    UObject* InputObject = GetSkeletonInputObject( AtIndex );
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

FReply
UHoudiniAssetInput::OnResetSkeletalMeshClicked( int32 AtIndex )
{
    OnSkeletalMeshDropped( nullptr, AtIndex );
    return FReply::Handled();
}

void
UHoudiniAssetInput::OnChoiceChange( TSharedPtr< FString > NewChoice )
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
    ChangeInputType( newChoice, false );
    MarkChanged();
}

bool
UHoudiniAssetInput::ChangeInputType(const EHoudiniAssetInputType::Enum& newType, const bool& ForceRefresh )
{
    if ( ChoiceIndex == newType && !ForceRefresh )
        return true;

    switch ( ChoiceIndex )
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
            break;
        }

        case EHoudiniAssetInputType::WorldInput:
        {
            // We are switching away from World Outliner input.
            // Stop monitoring the Actors for transform changes.
            StopWorldOutlinerTicking();

            break;
        }

        case EHoudiniAssetInputType::SkeletonInput:
        {
            // We are switching away from a skeleton input.
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
    switch ( newType )
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
            USceneComponent* RootComp = GetHoudiniAssetComponent();
            if( RootComp && !RootComp->IsPendingKill()
                && RootComp->GetOwner() && !RootComp->GetOwner()->IsPendingKill() )
            {
                if( !InputCurve || InputCurve->IsPendingKill() )
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
            if ( FHoudiniEngineUtils::HapiCreateInputNodeForWorldOutliner(
                HostAssetId, InputOutlinerMeshArray,
                ConnectedAssetId, CreatedInputDataAssetIds,
                UnrealSplineResolution ) )
            {
                ConnectInputNode();
            }

            break;
        }

        case EHoudiniAssetInputType::SkeletonInput:
        {
            // We are switching to skeleton input.
            if ( SkeletonInputObjects.Num() )
                bStaticMeshChanged = true;
            break;
        }

        default:
        {
            // Unhandled new input type?
            check(0);
            break;
        }
    }

    // Make sure the Input choice string corresponds to the selected input type
    if ( StringChoiceLabels.IsValidIndex( ChoiceIndex ) )
    {
        FString NewStringChoice = *( StringChoiceLabels[ ChoiceIndex ].Get() );
        if ( !ChoiceStringValue.Equals(NewStringChoice, ESearchCase::IgnoreCase ) )
            ChoiceStringValue = NewStringChoice;
    }

    // If we have input object and geometry asset, we need to connect it back.
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
    if ( ( !Actor || Actor->IsPendingKill() )
        && ( InputAssetComponent && !InputAssetComponent->IsPendingKill() ) )
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
        AHoudiniAssetActor * HoudiniAssetActor = (AHoudiniAssetActor *)Actor;
        if ( !HoudiniAssetActor || HoudiniAssetActor->IsPendingKill() )
            return;

        UHoudiniAssetComponent * ConnectedHoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
        if ( !ConnectedHoudiniAssetComponent || ConnectedHoudiniAssetComponent->IsPendingKill() )
            return;

        // If we just selected the already selected Actor do nothing if we are properly connected.
        if ( ConnectedHoudiniAssetComponent == InputAssetComponent && bInputAssetConnectedInHoudini)
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
        if ( InputAssetComponent && !InputAssetComponent->IsPendingKill() )
            InputAssetComponent->RemoveDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );

        InputAssetComponent = ConnectedHoudiniAssetComponent;
        ConnectedAssetId = InputAssetComponent->GetAssetId();

        // Do we have to wait for the input asset to cook?
        const UHoudiniAssetComponent* HAC = GetHoudiniAssetComponent();
        if ( HAC && !HAC->IsPendingKill() )
            GetHoudiniAssetComponent()->UpdateWaitingForUpstreamAssetsToInstantiate( true );

        // Mark as disconnected since we need to reconnect to the new asset.
        bInputAssetConnectedInHoudini = false;
    }

    MarkChanged();
}

void
UHoudiniAssetInput::OnLandscapeActorSelected( AActor * Actor )
{
    ALandscapeProxy * LandscapeProxy = Cast< ALandscapeProxy >( Actor );
    if ( LandscapeProxy && !LandscapeProxy->IsPendingKill() )
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
        InputLandscapeTransform = LandscapeProxy->ActorToWorld();
    }
    else
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Landscape Change." ),
            PrimaryObject );
        Modify();

        InputLandscapeProxy = nullptr;
        InputLandscapeTransform = FTransform::Identity;
    }

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
    // Do not tick non world inputs
    if (ChoiceIndex != EHoudiniAssetInputType::WorldInput)
    {
        return;
    }

    // Only tick/cook when in Editor
    // This prevents PIE cooks or runtime cooks due to inputs moving
    if (!GetWorld() || (GetWorld()->WorldType != EWorldType::Editor))
    {
        return;
    }

#if WITH_EDITOR
    // Stop outliner objects from causing recooks while input objects are dragged around
    if (FHoudiniMoveTracker::Get().IsObjectMoving)
    {
        return;
    }
#endif

    // PostLoad initialization must be done on the first tick
    // as some components might now have been fully initialized during PostLoad()
    if ( OutlinerInputsNeedPostLoadInit )
    {
        for (auto & OutlinerInput : InputOutlinerMeshArray)
        {
            if (OutlinerInput.ActorPtr.IsValid())
                continue;

            // Try to update the actor ptr via the pathname
            OutlinerInput.TryToUpdateActorPtrFromActorPathName();
        }

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

    // Don't do anything more if HEngine cooking is paused
    // We need to be able to detect updates/changes to the input actor when cooking is unpaused
    if ( !FHoudiniEngine::Get().GetEnableCookingGlobal() )
        return;

    // Lambda use to Modify / Prechange only once
    bool bLocalChanged = false;
    auto MarkLocalChanged = [&]()
    {
        if ( !bLocalChanged )
        {
            Modify();
            bLocalChanged = true;
        }
    };
    
    // Refresh the input's component from the actor
    // If the Actor is a blueprint, its component are recreated for every modification
    if ( UpdateInputOulinerArray() )
    {
        MarkLocalChanged();
        bStaticMeshChanged = true;
        MarkChanged();
    }

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
            FHoudiniApi::TransformEuler_Init(&HapiTransform);
            //FMemory::Memzero< HAPI_TransformEuler >( HapiTransform );
            FHoudiniEngineUtils::TranslateUnrealTransform( OutlinerInput.ComponentTransform, HapiTransform );

            HAPI_NodeInfo LocalAssetNodeInfo;
            FHoudiniApi::NodeInfo_Init(&LocalAssetNodeInfo);
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
        else if ( OutlinerInput.HasComponentMaterialsChanged() )
        {
            MarkLocalChanged();

            // Update the materials
            UpdateWorldOutlinerMaterials( OutlinerInput );

            // The materials have been changed so so we need to indicate that the "static mesh" 
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
    // Check the component we're connected to is valid
    if ( !InputAssetComponent )
        return;

    if ( !FHoudiniEngineUtils::IsValidNodeId( InputAssetComponent->GetAssetId() ) )
        return;

    // Check we have the correct Id
    if ( ConnectedAssetId != InputAssetComponent->GetAssetId() )
        ConnectedAssetId = InputAssetComponent->GetAssetId();

    // Connect if needed
    if ( !bInputAssetConnectedInHoudini )
    {
        InputAssetComponent->AddDownstreamAsset( GetHoudiniAssetComponent(), InputIndex );
        bInputAssetConnectedInHoudini = ConnectInputNode();
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
    if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) && ( ChoiceIndex == EHoudiniAssetInputType::GeometryInput ) )
    {
        for ( auto InputObject : InputObjects )
        {
            if ( InputObject && !InputObject->IsPendingKill() )
                return true;
        }
    }

    return false;
}

bool
UHoudiniAssetInput::IsInputAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId )
        && InputAssetComponent
        && !InputAssetComponent->IsPendingKill()
        && bInputAssetConnectedInHoudini )
    {
        if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
            return true;
    }

    return false;
}

bool
UHoudiniAssetInput::IsCurveAssetConnected() const
{
    if (InputCurve && !InputCurve->IsPendingKill() && ( ChoiceIndex == EHoudiniAssetInputType::CurveInput ) )
        return true;

    return false;
}

bool
UHoudiniAssetInput::IsLandscapeAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) )
    {
        if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
            return true;
    }

    return false;
}

bool
UHoudiniAssetInput::IsWorldInputAssetConnected() const
{
    if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) )
    {
        if ( ChoiceIndex == EHoudiniAssetInputType::WorldInput )
            return true;
    }

    return false;
}

void
UHoudiniAssetInput::OnInputCurveChanged()
{
    MarkChanged();
}

void
UHoudiniAssetInput::ExternalDisconnectInputAssetActor()
{
    InputAssetComponent = nullptr;
    ConnectedAssetId = -1;

    MarkChanged();
}

bool
UHoudiniAssetInput::DoesInputAssetNeedInstantiation()
{
    //if ( ChoiceIndex != EHoudiniAssetInputType::AssetInput )
    //    return false;

    if ( InputAssetComponent == nullptr )
        return false;

    if ( !FHoudiniEngineUtils::IsValidNodeId(InputAssetComponent->GetAssetId() ) )
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
        if ( FHoudiniEngineUtils::IsValidNodeId( ConnectedAssetId ) )
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
    FHoudiniApi::NodeInfo_Init(&NodeInfo);
    HOUDINI_CHECK_ERROR_RETURN(
        FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &NodeInfo),
        false);

    // Construct geo part object.
    FHoudiniGeoPartObject HoudiniGeoPartObject( ConnectedAssetId, NodeInfo.parentId, ConnectedAssetId, 0 );
    HoudiniGeoPartObject.bIsCurve = true;

    HAPI_AttributeInfo AttributeRefinedCurvePositions;
    FHoudiniApi::AttributeInfo_Init(&AttributeRefinedCurvePositions);
    //FMemory::Memzero< HAPI_AttributeInfo >( AttributeRefinedCurvePositions );

    TArray< float > RefinedCurvePositions;
    FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
        HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION,
        AttributeRefinedCurvePositions, RefinedCurvePositions );

    // Process coords string and extract positions.
    TArray< FVector > CurvePoints;
    FHoudiniEngineUtils::ExtractStringPositions( CurvePointsString, CurvePoints );

    TArray< FVector > CurveDisplayPoints;
    FHoudiniEngineUtils::ConvertScaleAndFlipVectorData( RefinedCurvePositions, CurveDisplayPoints );

    if (InputCurve && !InputCurve->IsPendingKill() )
    {
        InputCurve->Construct(
            HoudiniGeoPartObject, CurveDisplayPoints, CurveTypeValue, CurveMethodValue,
            (CurveClosed == 1));
    }

    // We also need to construct curve parameters we care about.
    TMap< FString, UHoudiniAssetParameter * > NewInputCurveParameters;

    TArray< HAPI_ParmInfo > ParmInfos;
    ParmInfos.SetNumUninitialized( NodeInfo.parmCount );
    for (int32 Idx = 0; Idx < ParmInfos.Num(); Idx++)
        FHoudiniApi::ParmInfo_Init(&(ParmInfos[Idx]));

    HOUDINI_CHECK_ERROR_RETURN(
        FHoudiniApi::GetParameters(
            FHoudiniEngine::Get().GetSession(), ConnectedAssetId, 
            &ParmInfos[ 0 ], 0, NodeInfo.parmCount ), false);

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
        UHoudiniAssetParameter * HoudiniAssetParameter = FoundHoudiniAssetParameter ? *FoundHoudiniAssetParameter : nullptr;

        // If parameter exists, we can reuse it.
        if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsPendingKill() )
        {
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

            if ( HoudiniAssetParameter && !HoudiniAssetParameter->IsPendingKill() )
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
UHoudiniAssetInput::CheckStateChangedExportAllLODs( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bExportAllLODs == bState )
        return;

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_RUNTIME),
        LOCTEXT("HoudiniInputChange", "Houdini Input Export all LODs changed."),
        PrimaryObject);
    Modify();

    bExportAllLODs = bState;

    // Changing the export of LODs changes the StaticMesh!
    if ( HasLODs() )
        bStaticMeshChanged = true;

    // Mark this parameter as changed.
    MarkChanged();
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportAllLODs() const
{
    if ( bExportAllLODs )
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedExportSockets( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bExportSockets == bState )
        return;

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input export sockets changed." ),
        PrimaryObject );
    Modify();

    bExportSockets = bState;

    // Changing the export of LODs changes the StaticMesh!
    if ( HasSockets() )
        bStaticMeshChanged = true;

    // Mark this parameter as changed.
    MarkChanged();
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedExportSockets() const
{
    if (bExportSockets)
        return ECheckBoxState::Checked;

    return ECheckBoxState::Unchecked;
}

void
UHoudiniAssetInput::CheckStateChangedUpdateInputLandscape( ECheckBoxState NewState )
{
    int32 bState = ( NewState == ECheckBoxState::Checked );

    if ( bUpdateInputLandscape == bState )
        return;

    // Record undo information.
    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_RUNTIME),
        LOCTEXT("HoudiniInputChange", "Houdini Input Update Input Landscape changed."),
        PrimaryObject);
    Modify();

    UHoudiniAssetComponent* ParentComponent = GetHoudiniAssetComponent();
    if ( InputLandscapeProxy && ParentComponent )
    {        
        if ( bState )
        {
            // Build the backup file name
            FString BackupBaseName = ParentComponent->GetTempCookFolder().ToString()
                + TEXT("/")
                + InputLandscapeProxy->GetName()
                + TEXT("_")
                + ParentComponent->GetComponentGuid().ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);

            // We need to cache the input landscape to a file
            //FString BaseName = TEXT("/Game/HoudiniEngine/Temp/LandscapeBak");
            FHoudiniLandscapeUtils::BackupLandscapeToFile( BackupBaseName, InputLandscapeProxy.Get());
            InputLandscapeTransform = InputLandscapeProxy->ActorToWorld();
        }
        else
        {
            // Detach the input landscape from the HDA            
            InputLandscapeProxy->DetachFromActor( FDetachmentTransformRules::KeepWorldTransform );

            // Clear the landscape map to avoid reusing the input landscape
            ParentComponent->ClearLandscapes();

            // Restore the input landscape's backup data
            FHoudiniLandscapeUtils::RestoreLandscapeFromFile( InputLandscapeProxy.Get());

            // Reapply the source Landscape's transform
            InputLandscapeProxy->SetActorTransform(InputLandscapeTransform);
        }
    }

    bUpdateInputLandscape = bState;

    // Mark this parameter as changed.
    MarkChanged();
}

ECheckBoxState
UHoudiniAssetInput::IsCheckedUpdateInputLandscape() const
{
    if ( bUpdateInputLandscape )
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

void 
UHoudiniAssetInput::OnInsertGeo( int32 AtIndex )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
        PrimaryObject );
    Modify();
    InputObjects.Insert( nullptr, AtIndex );
    InputTransforms.Insert( FTransform::Identity, AtIndex );
    TransformUIExpanded.Insert( false, AtIndex );
    bStaticMeshChanged = true;
    MarkChanged();
    OnParamStateChanged();
}

void 
UHoudiniAssetInput::OnDeleteGeo( int32 AtIndex )
{
    if ( ensure( InputObjects.IsValidIndex( AtIndex ) && InputTransforms.IsValidIndex( AtIndex ) ) )
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
            PrimaryObject );
        Modify();
        InputObjects.RemoveAt( AtIndex );
        InputTransforms.RemoveAt( AtIndex );
        TransformUIExpanded.RemoveAt( AtIndex );
        bStaticMeshChanged = true;
        MarkChanged();
        OnParamStateChanged();
    }
}

void 
UHoudiniAssetInput::OnDuplicateGeo( int32 AtIndex )
{
    if ( ensure( InputObjects.IsValidIndex( AtIndex ) ) && InputTransforms.IsValidIndex( AtIndex ) )
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
            PrimaryObject );
        Modify();
        UObject* Dupe = InputObjects[AtIndex];
        InputObjects.Insert( Dupe, AtIndex );
        FTransform DupeTransform = InputTransforms[AtIndex];
        InputTransforms.Insert( DupeTransform, AtIndex );
        bool DupeUIExpanded = TransformUIExpanded[AtIndex];
        TransformUIExpanded.Insert( DupeUIExpanded, AtIndex );
        bStaticMeshChanged = true;
        MarkChanged();
        OnParamStateChanged();
    }
}

FReply
UHoudiniAssetInput::OnButtonClickRecommit()
{
    // There's no undo operation for button.
    MarkChanged();

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
    if (!InputCurve || InputCurve->IsPendingKill() )
        return;

    if (!OriginalInput || OriginalInput->IsPendingKill())
        return;

    USceneComponent* RootComp = GetHoudiniAssetComponent();
    if( !RootComp || RootComp->IsPendingKill() )
        return;

    if ( !RootComp->GetOwner() || RootComp->GetOwner()->IsPendingKill() )
        return;

    // The previous call to DuplicateObject did not duplicate the curves properly
    // Both the original and duplicated Inputs now share the same InputCurve, so we 
    // need to create a proper copy of that curve

    // Keep the original pointer to the curve, as we need to duplicate its data
    UHoudiniSplineComponent* OriginalCurve = InputCurve;

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
    OriginalCurve->SetHoudiniAssetInput(OriginalInput);

    // "Copy" the old curves parameters to the new one
    InputCurve->CopyFrom(OriginalCurve);

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

    bStaticMeshChanged = true;
    InputOutlinerMeshArray.RemoveAt( AtIndex );
    MarkChanged();
}

void
UHoudiniAssetInput::UpdateWorldOutlinerTransforms( FHoudiniAssetInputOutlinerMesh& OutlinerMesh )
{
    // Update to the new Transforms
    if ( OutlinerMesh.ActorPtr.IsValid() )
        OutlinerMesh.ActorTransform = OutlinerMesh.ActorPtr->GetTransform();

    if ( OutlinerMesh.StaticMeshComponent && !OutlinerMesh.StaticMeshComponent->IsPendingKill() )
    {
        OutlinerMesh.ComponentTransform = OutlinerMesh.StaticMeshComponent->GetComponentTransform();

        // Handle instances here
        UInstancedStaticMeshComponent * InstancedStaticMeshComponent = Cast< UInstancedStaticMeshComponent >(OutlinerMesh.StaticMeshComponent);
        if (InstancedStaticMeshComponent)
        {
            FTransform InstanceTransform;
            if (InstancedStaticMeshComponent->GetInstanceTransform(OutlinerMesh.InstanceIndex, InstanceTransform, true))
                OutlinerMesh.ComponentTransform = InstanceTransform;
        }
    }
    else if (OutlinerMesh.SplineComponent && !OutlinerMesh.SplineComponent->IsPendingKill() )
    {
        OutlinerMesh.ComponentTransform = OutlinerMesh.SplineComponent->GetComponentTransform();
    }

    OutlinerMesh.KeepWorldTransform = bKeepWorldTransform;
}

void
UHoudiniAssetInput::UpdateWorldOutlinerMaterials(FHoudiniAssetInputOutlinerMesh& OutlinerMesh)
{
    OutlinerMesh.MeshComponentsMaterials.Empty();
    if ( !OutlinerMesh.StaticMeshComponent || OutlinerMesh.StaticMeshComponent->IsPendingKill() )
        return;

    // Keep track of the materials used by the SMC
    for ( int32 n = 0; n < OutlinerMesh.StaticMeshComponent->GetNumMaterials(); n++ )
    {
        UMaterialInterface* mi = OutlinerMesh.StaticMeshComponent->GetMaterial( n );
        if ( !mi || mi->IsPendingKill() )
            OutlinerMesh.MeshComponentsMaterials.Add( FString() );
        else
            OutlinerMesh.MeshComponentsMaterials.Add( mi->GetPathName() );
    }
}

void UHoudiniAssetInput::OnAddToInputObjects()
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInputChange", "Houdini Input Geometry Change" ),
        PrimaryObject );
    Modify();
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

void UHoudiniAssetInput::OnAddToSkeletonInputObjects()
{
    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_RUNTIME),
        LOCTEXT("HoudiniInputChange", "Houdini Input Geometry Change"),
        PrimaryObject);
    Modify();
    SkeletonInputObjects.Add(nullptr);
    //InputTransforms.Add(FTransform::Identity);
    //TransformUIExpanded.Add(false);
    MarkChanged();
    bStaticMeshChanged = true;
    OnParamStateChanged();
}

void UHoudiniAssetInput::OnEmptySkeletonInputObjects()
{
    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_RUNTIME),
        LOCTEXT("HoudiniInputChange", "Houdini Input Geometry Change"),
        PrimaryObject);
    Modify();

    // Empty the arrays
    SkeletonInputObjects.Empty();
    //InputTransforms.Empty();
    //TransformUIExpanded.Empty();

    // To avoid displaying 0 elements when there's one (empty), initialize the array
    SkeletonInputObjects.Add(nullptr);
    //InputTransforms.Add(FTransform::Identity);
    //TransformUIExpanded.Add(false);
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
        if (InputOutlinerMeshArray[n].SplineComponent && !InputOutlinerMeshArray[n].SplineComponent->IsPendingKill() )
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

FText
UHoudiniAssetInput::GetCurrentSelectionText() const
{
    FText CurrentSelectionText;
    if ( ChoiceIndex == EHoudiniAssetInputType::AssetInput )
    {
        if ( InputAssetComponent && !InputAssetComponent->IsPendingKill() && InputAssetComponent->GetHoudiniAssetActorOwner() )
        {
            CurrentSelectionText = FText::FromString( InputAssetComponent->GetHoudiniAssetActorOwner()->GetName() );
        }
    }
    else if ( ChoiceIndex == EHoudiniAssetInputType::LandscapeInput )
    {
        if ( InputLandscapeProxy && !InputLandscapeProxy->IsPendingKill() )
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
UHoudiniAssetInput::GetInputBounds( const FVector& ParentLocation ) const
{
    FBox Bounds( ForceInitToZero );

    if ( IsCurveAssetConnected() && InputCurve && !InputCurve->IsPendingKill() )
    {
        // Houdini Curves are expressed locally so we need to add the parent component's transform
        TArray<FVector> CurvePositions;
        InputCurve->GetCurvePositions( CurvePositions );

        for ( int32 n = 0; n < CurvePositions.Num(); n++ )
            Bounds += ( ParentLocation + CurvePositions[ n ] );
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

    if ( IsInputAssetConnected() && InputAssetComponent && !InputAssetComponent->IsPendingKill() )
    {
        Bounds += InputAssetComponent->GetAssetBounds();
    }

    if ( IsLandscapeAssetConnected() && InputLandscapeProxy && !InputLandscapeProxy->IsPendingKill() )
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
    FString curvePrefix        = TEXT( "curve" );
    FString landscapePrefix    = TEXT( "landscape" );
    FString landscapePrefix2    = TEXT( "terrain" );
    FString landscapePrefix3    = TEXT( "heightfield" );
    FString worldPrefix        = TEXT( "world" );
    FString worldPrefix2    = TEXT( "outliner" );
    FString assetPrefix        = TEXT( "asset" );
    FString assetPrefix2    = TEXT( "hda" );

    // By default, geometry input is chosen.
    EHoudiniAssetInputType::Enum newInputType = EHoudiniAssetInputType::GeometryInput;

    if ( inputName.Contains( curvePrefix, ESearchCase::IgnoreCase ) )
        newInputType = EHoudiniAssetInputType::CurveInput;

    else if ( ( inputName.Contains( landscapePrefix, ESearchCase::IgnoreCase ) ) 
            || ( inputName.Contains( landscapePrefix2, ESearchCase::IgnoreCase ) )
            || ( inputName.Contains( landscapePrefix3, ESearchCase::IgnoreCase ) ) )
        newInputType = EHoudiniAssetInputType::LandscapeInput;

    else if ( ( inputName.Contains( worldPrefix, ESearchCase::IgnoreCase ) )
            || ( inputName.Contains( worldPrefix2, ESearchCase::IgnoreCase ) ) )
        newInputType = EHoudiniAssetInputType::WorldInput;

    else if ( ( inputName.Contains( assetPrefix, ESearchCase::IgnoreCase ) )
            || ( inputName.Contains( assetPrefix2, ESearchCase::IgnoreCase ) ) )
        newInputType = EHoudiniAssetInputType::AssetInput;

    if ( ChoiceIndex != newInputType )
        ChangeInputType( newInputType, false );

#else
    ChoiceIndex = EHoudiniAssetInputType::GeometryInput;
#endif
}

bool
UHoudiniAssetInput::HasChanged() const
{
    // Inputs should be considered changed after being loaded
    return bChanged || bLoadedParameter || ( !bInputAssetConnectedInHoudini && ChoiceIndex == EHoudiniAssetInputType::AssetInput );
}

#if WITH_EDITOR
bool
UHoudiniAssetInput::UpdateInputOulinerArray()
{
    bool NeedsUpdate = false;

    // See if some outliner inputs need to be updated, or removed
    // If an input's Actor is no longer valid, then when need to remove that input.
    // If an input's Components needs to be updated (are no longer valid), then all the components 
    // from the same actor needs to be updated as well.
    // This can happen for example when a blueprint actor is modified/serialized etc..
    TArray<AActor *> ActorToUpdateArray;
    for ( int32 n = InputOutlinerMeshArray.Num() - 1; n >= 0; n-- )
    {
        FHoudiniAssetInputOutlinerMesh& OutlinerInput = InputOutlinerMeshArray[ n ];
        if (OutlinerInput.ActorPtr.IsStale()) // || !OutlinerInput.ActorPtr.IsValid());
        {
            // If our ActorPtr is stale, try to find an updated version of it by pathname
            // This can happen when a blueprint is updated or recompiled...
            OutlinerInput.TryToUpdateActorPtrFromActorPathName();
        }
        
        if ( !OutlinerInput.ActorPtr.IsValid() )
        {
            // This input has an invalid actor: destroy it and its asset
            NeedsUpdate = true;

            // Destroy Houdini asset
            if ( FHoudiniEngineUtils::IsValidNodeId( OutlinerInput.AssetId ) )
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

            NeedsUpdate = true;
        }
    }

    // Creates the inputs from the actors
    for ( auto & CurrentActor : ActorToUpdateArray )
        UpdateInputOulinerArrayFromActor( CurrentActor, true );

    return NeedsUpdate;
}

void
UHoudiniAssetInput::UpdateInputOulinerArrayFromActor( AActor * Actor, const bool& NeedCleanUp )
{
    if ( !Actor )
        return;

    // Don't allow selection of ourselves. Bad things happen if we do.
    if ( GetHoudiniAssetComponent() && ( Actor == GetHoudiniAssetComponent()->GetOwner() ) )
        return;

    // Destroy previous outliner inputs linked to this actor if needed
    if (NeedCleanUp)
    {
        for (int32 n = InputOutlinerMeshArray.Num() - 1; n >= 0; n--)
        {
            if (InputOutlinerMeshArray[n].ActorPtr.Get() != Actor)
                continue;

            // Remove from the input array.
            // No need to destroy the houdini nodes since this will be taken care of
            // when we update the input. (see bug 95415)
            InputOutlinerMeshArray.RemoveAt(n);
        }
    }

    // Looking for StaticMeshes
    TArray< UStaticMeshComponent* > AllSMComponents;
    Actor->GetComponents<UStaticMeshComponent>(AllSMComponents);
    for (UStaticMeshComponent * StaticMeshComponent : AllSMComponents)
    {
        if ( !StaticMeshComponent || StaticMeshComponent->ComponentHasTag( NAME_HoudiniNoUpload ) )
            continue;

        UStaticMesh * StaticMesh = StaticMeshComponent->GetStaticMesh();
        if ( !StaticMesh )
            continue;

        UInstancedStaticMeshComponent * InstancedStaticMeshComponent = Cast< UInstancedStaticMeshComponent >(StaticMeshComponent);
        if (InstancedStaticMeshComponent)
        {
            // Handle ISM separately
            // We'll create an Outliner Mesh for each of the instances..
            for (int32 n = 0; n < InstancedStaticMeshComponent->GetInstanceCount(); n++)
            {
                // Add the mesh to the array
                FHoudiniAssetInputOutlinerMesh OutlinerMesh;
                OutlinerMesh.ActorPtr = Actor;
                OutlinerMesh.ActorPathName = Actor->GetPathName();
                OutlinerMesh.StaticMeshComponent = InstancedStaticMeshComponent;
                OutlinerMesh.StaticMesh = StaticMesh;
                OutlinerMesh.SplineComponent = nullptr;
                OutlinerMesh.AssetId = -1;
                OutlinerMesh.InstanceIndex = n;

                UpdateWorldOutlinerTransforms(OutlinerMesh);
                UpdateWorldOutlinerMaterials(OutlinerMesh);

                InputOutlinerMeshArray.Add(OutlinerMesh);
            }
        }
        else
        {
            // Add the Static Mesh to the array
            FHoudiniAssetInputOutlinerMesh OutlinerMesh;
            OutlinerMesh.ActorPtr = Actor;
            OutlinerMesh.ActorPathName = Actor->GetPathName();
            OutlinerMesh.StaticMeshComponent = StaticMeshComponent;
            OutlinerMesh.StaticMesh = StaticMesh;
            OutlinerMesh.SplineComponent = nullptr;
            OutlinerMesh.AssetId = -1;

            UpdateWorldOutlinerTransforms(OutlinerMesh);
            UpdateWorldOutlinerMaterials(OutlinerMesh);

            InputOutlinerMeshArray.Add(OutlinerMesh);
        }
    }

    // Looking for Splines
    TArray< USplineComponent* > AllSplineComponents;
    Actor->GetComponents<USplineComponent>(AllSplineComponents);
    for (USplineComponent * SplineComponent : AllSplineComponents)
    {
        if ( !SplineComponent || SplineComponent->ComponentHasTag( NAME_HoudiniNoUpload ) )
            continue;

        // Add the spline to the array
        FHoudiniAssetInputOutlinerMesh OutlinerMesh;

        OutlinerMesh.ActorPtr = Actor;
        OutlinerMesh.ActorPathName = Actor->GetPathName();
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

bool
UHoudiniAssetInput::AddInputObject( UObject* ObjectToAdd )
{
    if ( !ObjectToAdd || ObjectToAdd->IsPendingKill() )
        return false;

    // Fix for the bug due to the first (but null) geometry input mesh
    int32 IndexToAdd = InputObjects.Num();
    if ( IndexToAdd == 1 && ( InputObjects[ 0 ] == nullptr ) )
        IndexToAdd = 0;

    if ( UStaticMesh* StaticMesh = Cast< UStaticMesh >( ObjectToAdd ) )
    {
        ForceSetInputObject( ObjectToAdd, IndexToAdd, true );
        return true;
    }
    else if ( AActor* WorldActor = Cast< AActor >( ObjectToAdd ) )
    {
        ForceSetInputObject( ObjectToAdd, IndexToAdd, true );
        return true;
    }

    //if ( InputAssetComponent )
    //    InputAssetComponent->bEditorPropertiesNeedFullUpdate = true;

    return false;
}

#endif

ALandscapeProxy*
UHoudiniAssetInput::GetLandscapeInput()
{
    if (!InputLandscapeProxy || InputLandscapeProxy->IsPendingKill())
        return nullptr;

    return InputLandscapeProxy->GetLandscapeActor();
}

bool
UHoudiniAssetInput::HasLODs() const
{
    switch ( ChoiceIndex )
    {
        case EHoudiniAssetInputType::GeometryInput:
        {
            if ( !InputObjects.Num() )
                return false;

            for ( int32 Idx = 0; Idx < InputObjects.Num(); Idx++ )
            {
                UStaticMesh* SM = Cast<UStaticMesh>( InputObjects[ Idx ] );
                if ( !SM || SM->IsPendingKill() )
                    continue;

                if ( SM->GetNumLODs() > 1 )
                    return true;
            }
        }
        break;

        case EHoudiniAssetInputType::WorldInput:
        {
            if ( !InputOutlinerMeshArray.Num() )
                return false;

            for ( int32 Idx = 0; Idx < InputOutlinerMeshArray.Num(); Idx++ )
            {
                UStaticMesh* SM = InputOutlinerMeshArray[ Idx ].StaticMesh;
                if ( !SM || SM->IsPendingKill() )
                    continue;

                if ( SM->GetNumLODs() > 1 )
                    return true;
            }
        }
        break;
    }

    return false;
}

bool
UHoudiniAssetInput::HasSockets() const
{
    switch ( ChoiceIndex )
    {
        case EHoudiniAssetInputType::GeometryInput:
        {
            if ( !InputObjects.Num() )
                return false;

            for ( int32 Idx = 0; Idx < InputObjects.Num(); Idx++ )
            {
                UStaticMesh* SM = Cast<UStaticMesh>( InputObjects[ Idx ] );
                if ( !SM )
                    continue;

                if ( SM->Sockets.Num() > 0 )
                    return true;
            }
        }
        break;

        case EHoudiniAssetInputType::WorldInput:
        {
            if ( !InputOutlinerMeshArray.Num() )
                return false;

            for ( int32 Idx = 0; Idx < InputOutlinerMeshArray.Num(); Idx++ )
            {
                UStaticMesh* SM = InputOutlinerMeshArray[ Idx ].StaticMesh;
                if ( !SM )
                    continue;

                if ( SM->Sockets.Num() > 1 )
                    return true;
            }
        }
        break;
    }

    return false;
}

bool
UHoudiniAssetInput::IsLandscapeUpdateNeededOnTransformChange() const
{
    if ( GetChoiceIndex() != EHoudiniAssetInputType::LandscapeInput )
        return false;

    if ( !bLandscapeAutoSelectComponent || !bLandscapeExportSelectionOnly )
        return false;

    return true;
}

bool
UHoudiniAssetInput::SetDefaultAssetFromHDA()
{
#if WITH_EDITOR
    // We just handle geo inputs
    if (EHoudiniAssetInputType::GeometryInput != ChoiceIndex)
        return false;

    // There is a default slot, don't add if slot is already filled 
    if (InputObjects.Num() > 1)
        return false;

    // Make sure we're linked to a valid parameter
    if (ParmId < 0)
        return false;

    // Get our ParmInfo
    HAPI_ParmInfo FoundParamInfo;
    FHoudiniApi::ParmInfo_Init(&FoundParamInfo);
    if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetParmInfo(
        FHoudiniEngine::Get().GetSession(),
        NodeId, ParmId, &FoundParamInfo) )
    {
        return false;
    }
    
    // Get our string value
    HAPI_StringHandle StringHandle;
    if (FHoudiniApi::GetParmStringValues(
        FHoudiniEngine::Get().GetSession(), NodeId, false,
        &StringHandle, FoundParamInfo.stringValuesIndex, 1) == HAPI_RESULT_SUCCESS)
    {
        FString OutValue;
        FHoudiniEngineString HoudiniEngineString(StringHandle);
        if (HoudiniEngineString.ToFString(OutValue))
        {
            // Set default object on the HDA instance - will override the parameter string
            // and apply the object input local-path thing for the HDA cook.
            if (OutValue.Len() > 0)
            {
                UObject * pObject = LoadObject<UObject>(nullptr, *OutValue);
                if (pObject)
                {
                    return AddInputObject(pObject);
                }
            }
        }
    }
    
#endif //WITH EDITOR
    return false;
}

#undef LOCTEXT_NAMESPACE