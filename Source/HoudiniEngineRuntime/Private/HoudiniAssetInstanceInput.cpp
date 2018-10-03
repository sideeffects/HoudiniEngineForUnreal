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

#include "HoudiniApi.h"
#include "HoudiniAssetInstanceInput.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetInstanceInputField.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"

#include "Internationalization/Internationalization.h"
#include "HoudiniEngineBakeUtils.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 


UHoudiniAssetInstanceInput::UHoudiniAssetInstanceInput( const FObjectInitializer& ObjectInitializer )
    : Super( ObjectInitializer )
    , ObjectToInstanceId( -1 )
{
    Flags.HoudiniAssetInstanceInputFlagsPacked = 0;
    TupleSize = 0;
}

UHoudiniAssetInstanceInput *
UHoudiniAssetInstanceInput::Create(
    UHoudiniAssetComponent * InPrimaryObject,
    const FHoudiniGeoPartObject & InHoudiniGeoPartObject )
{
    UHoudiniAssetInstanceInput * NewInstanceInput = nullptr;

    // Get object to be instanced.
    HAPI_NodeId ObjectToInstance = InHoudiniGeoPartObject.HapiObjectGetToInstanceId();

    auto Flags = GetInstancerFlags(InHoudiniGeoPartObject);

    // This is invalid combination, no object to instance and input is not an attribute instancer.
    if ( !Flags.bIsAttributeInstancer && !Flags.bAttributeInstancerOverride && ObjectToInstance == -1 && !Flags.bIsPackedPrimitiveInstancer )
        return nullptr;

    NewInstanceInput = NewObject< UHoudiniAssetInstanceInput >(
        InPrimaryObject, 
        UHoudiniAssetInstanceInput::StaticClass(),
        NAME_None, RF_Public | RF_Transactional );

    NewInstanceInput->PrimaryObject = InPrimaryObject;
    NewInstanceInput->HoudiniGeoPartObject = InHoudiniGeoPartObject;
    NewInstanceInput->SetNameAndLabel( InHoudiniGeoPartObject.ObjectName );
    NewInstanceInput->ObjectToInstanceId = ObjectToInstance;

    NewInstanceInput->Flags = Flags;

    return NewInstanceInput;
}

UHoudiniAssetInstanceInput::FHoudiniAssetInstanceInputFlags
UHoudiniAssetInstanceInput::GetInstancerFlags(const FHoudiniGeoPartObject & InHoudiniGeoPartObject)
{
    FHoudiniAssetInstanceInputFlags Flags;
    Flags.HoudiniAssetInstanceInputFlagsPacked = 0;

    // Get object to be instanced.
    HAPI_NodeId ObjectToInstance = InHoudiniGeoPartObject.HapiObjectGetToInstanceId();

    Flags.bIsPackedPrimitiveInstancer = InHoudiniGeoPartObject.IsPackedPrimitiveInstancer();

    // If this is an attribute instancer, see if attribute exists.
    Flags.bIsAttributeInstancer = InHoudiniGeoPartObject.IsAttributeInstancer();

    // Check if this is an attribute override instancer (on detail or point).
    Flags.bAttributeInstancerOverride =	InHoudiniGeoPartObject.IsAttributeOverrideInstancer();

    // Check if this is a No-Instancers ( unreal_split_instances )
    Flags.bIsSplitMeshInstancer =
        InHoudiniGeoPartObject.HapiCheckAttributeExistance(
            HAPI_UNREAL_ATTRIB_SPLIT_INSTANCES, HAPI_ATTROWNER_DETAIL );
    return Flags;
}

UHoudiniAssetInstanceInput *
UHoudiniAssetInstanceInput::Create(
    UHoudiniAssetComponent * InPrimaryObject,
    const UHoudiniAssetInstanceInput * OtherInstanceInput )
{
    UHoudiniAssetInstanceInput * HoudiniAssetInstanceInput = 
        DuplicateObject( OtherInstanceInput, InPrimaryObject );

    // We need to duplicate field objects manually
    HoudiniAssetInstanceInput->InstanceInputFields.Empty();
    for ( const auto& OtherField : OtherInstanceInput->InstanceInputFields )
    {
        UHoudiniAssetInstanceInputField * NewField =
            UHoudiniAssetInstanceInputField::Create( InPrimaryObject, OtherField );

        HoudiniAssetInstanceInput->InstanceInputFields.Add( NewField );
    }
    // Fix the back-reference to the component
    HoudiniAssetInstanceInput->PrimaryObject = InPrimaryObject;
    return HoudiniAssetInstanceInput;
}

bool
UHoudiniAssetInstanceInput::CreateInstanceInput()
{
    if ( !PrimaryObject )
        return false;

    HAPI_NodeId AssetId = GetAssetId();

    // Retrieve instance transforms (for each point).
    TArray< FTransform > AllTransforms;
    HoudiniGeoPartObject.HapiGetInstanceTransforms( AssetId, AllTransforms );

    // List of new fields. Reused input fields will also be placed here.
    TArray< UHoudiniAssetInstanceInputField * > NewInstanceInputFields;

    if ( Flags.bIsPackedPrimitiveInstancer )
    {
        // This is using packed primitives
        HAPI_PartInfo PartInfo;

        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetPartInfo(
            FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId,
            &PartInfo ), false );

        // Retrieve part name.
        FString PartName;
        FHoudiniEngineString HoudiniEngineStringPartName( PartInfo.nameSH );
        HoudiniEngineStringPartName.ToFString( PartName );

        //HOUDINI_LOG_MESSAGE( TEXT( "Part Instancer (%s): IPC=%d, IC=%d" ), *PartName, PartInfo.instancedPartCount, PartInfo.instanceCount );

        // Get transforms for each instance
        TArray<HAPI_Transform> InstancerPartTransforms;
        InstancerPartTransforms.SetNumZeroed( PartInfo.instanceCount );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstancerPartTransforms(
            FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, PartInfo.id,
            HAPI_RSTORDER_DEFAULT, InstancerPartTransforms.GetData(), 0, PartInfo.instanceCount ), false );

        // Get the part ids for parts being instanced
        TArray<HAPI_PartId> InstancedPartIds;
        InstancedPartIds.SetNumZeroed( PartInfo.instancedPartCount );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstancedPartIds(
            FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, PartInfo.id,
            InstancedPartIds.GetData(), 0, PartInfo.instancedPartCount ), false );

        for ( auto InstancedPartId : InstancedPartIds )
        {
            HAPI_PartInfo InstancedPartInfo;
            HOUDINI_CHECK_ERROR_RETURN(
                FHoudiniApi::GetPartInfo(
                    FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, InstancedPartId,
                    &InstancedPartInfo ), false );

            TArray<FTransform> ObjectTransforms;
            ObjectTransforms.SetNumUninitialized( InstancerPartTransforms.Num() );
            for ( int32 InstanceIdx = 0; InstanceIdx < InstancerPartTransforms.Num(); ++InstanceIdx )
            {
                const auto& InstanceTransform = InstancerPartTransforms[InstanceIdx];
                FHoudiniEngineUtils::TranslateHapiTransform( InstanceTransform, ObjectTransforms[InstanceIdx] );
                //HOUDINI_LOG_MESSAGE( TEXT( "Instance %d:%d Transform: %s for Part Id %d" ), HoudiniGeoPartObject.PartId, InstanceIdx, *ObjectTransforms[ InstanceIdx ].ToString(), InstancedPartId );
            }

            // Create this instanced input field for this instanced part
            //
            FHoudiniGeoPartObject InstancedPart( HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, InstancedPartId );
            InstancedPart.TransformMatrix = HoudiniGeoPartObject.TransformMatrix;
            CreateInstanceInputField( InstancedPart, ObjectTransforms, InstanceInputFields, NewInstanceInputFields );
        }
    }
    else if ( Flags.bIsAttributeInstancer )
    {
        int32 NumPoints = HoudiniGeoPartObject.HapiPartGetPointCount();
        TArray< HAPI_NodeId > InstancedObjectIds;
        InstancedObjectIds.SetNumUninitialized( NumPoints );
        HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetInstancedObjectIds(
            FHoudiniEngine::Get().GetSession(), HoudiniGeoPartObject.GeoId, 
            InstancedObjectIds.GetData(), 
            0, NumPoints), false );

        // Find the set of instanced object ids and locate the corresponding parts
        TSet< int32 > UniqueInstancedObjectIds( InstancedObjectIds );
        TArray< FTransform > InstanceTransforms;
        for ( int32 InstancedObjectId : UniqueInstancedObjectIds )
        {
            if( UHoudiniAssetComponent* Comp = GetHoudiniAssetComponent() )
            {
                TArray< FHoudiniGeoPartObject > PartsToInstance;
                if( Comp->LocateStaticMeshes( InstancedObjectId, PartsToInstance ) )
                {
                    // copy out the transforms for this instance id
                    InstanceTransforms.Empty();
                    for( int32 Ix = 0; Ix < InstancedObjectIds.Num(); ++Ix )
                    {
                        if( ( InstancedObjectIds[ Ix ] == InstancedObjectId ) && ( AllTransforms.IsValidIndex( Ix ) ) )
                        {
                            InstanceTransforms.Add( AllTransforms[ Ix ] );
                        }
                    }

                    // Locate or create an instance input field for each part for this instanced object id
                    for( FHoudiniGeoPartObject& Part : PartsToInstance )
                    {
                        // Change the transform of the part being instanced to match the instancer
                        Part.TransformMatrix = HoudiniGeoPartObject.TransformMatrix;
                        CreateInstanceInputField( Part, InstanceTransforms, InstanceInputFields, NewInstanceInputFields );
                    }
                }
            }
        }
    }
    else if ( Flags.bAttributeInstancerOverride )
    {
        // This is an attribute override. Unreal mesh is specified through an attribute and we use points.

        std::string MarshallingAttributeInstanceOverride = HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE;
        UHoudiniRuntimeSettings::GetSettingsValue(
            TEXT( "MarshallingAttributeInstanceOverride" ),
            MarshallingAttributeInstanceOverride );

        HAPI_AttributeInfo ResultAttributeInfo;
        FMemory::Memzero< HAPI_AttributeInfo >( ResultAttributeInfo );

        if ( !HoudiniGeoPartObject.HapiGetAttributeInfo(
            AssetId, MarshallingAttributeInstanceOverride,
            ResultAttributeInfo ) )
        {
            // We had an error while retrieving the attribute info.
            return false;
        }

        if ( !ResultAttributeInfo.exists )
        {
            // Attribute does not exist.
            return false;
        }

        if ( ResultAttributeInfo.owner == HAPI_ATTROWNER_DETAIL )
        {
            // Attribute is on detail, this means it gets applied to all points.

            TArray< FString > DetailInstanceValues;

            if ( !HoudiniGeoPartObject.HapiGetAttributeDataAsString(
                AssetId, MarshallingAttributeInstanceOverride,
                HAPI_ATTROWNER_DETAIL, ResultAttributeInfo, DetailInstanceValues ) )
            {
                // This should not happen - attribute exists, but there was an error retrieving it.
                return false;
            }

            if ( DetailInstanceValues.Num() == 0 )
            {
                // No values specified.
                return false;
            }

            // Attempt to load specified asset.
            const FString & AssetName = DetailInstanceValues[ 0 ];
            UObject * AttributeObject =
                StaticLoadObject( 
                    UObject::StaticClass(), nullptr, *AssetName, nullptr, LOAD_None, nullptr );

            if ( AttributeObject )
            {
                CreateInstanceInputField(
                    AttributeObject, AllTransforms, InstanceInputFields,
                    NewInstanceInputFields );
            }
            else
            {
                return false;
            }
        }
        else if ( ResultAttributeInfo.owner == HAPI_ATTROWNER_POINT )
        {
            TArray< FString > PointInstanceValues;

            if ( !HoudiniGeoPartObject.HapiGetAttributeDataAsString(
                AssetId, MarshallingAttributeInstanceOverride,
                HAPI_ATTROWNER_POINT, ResultAttributeInfo, PointInstanceValues ) )
            {
                // This should not happen - attribute exists, but there was an error retrieving it.
                return false;
            }

            // Attribute is on points, number of points must match number of transforms.
            if ( PointInstanceValues.Num() != AllTransforms.Num() )
            {
                // This should not happen, we have mismatch between number of instance values and transforms.
                return false;
            }

            // If instance attribute exists on points, we need to get all unique values.
            TMap< FString, UObject * > ObjectsToInstance;

            for ( auto Iter : PointInstanceValues )
            {
                const FString & UniqueName = *Iter;

                if ( !ObjectsToInstance.Contains( UniqueName ) )
                {
                    UObject * AttributeObject = StaticLoadObject( 
                        UObject::StaticClass(), nullptr, *UniqueName, nullptr, LOAD_None, nullptr );

                    ObjectsToInstance.Add( UniqueName, AttributeObject );
                }
            }

            bool Success = false;

            for( auto Iter : ObjectsToInstance )
            {
                const FString & InstancePath = Iter.Key;
                UObject * AttributeObject = Iter.Value;

                if ( AttributeObject )
                {
                    TArray< FTransform > ObjectTransforms;
                    GetPathInstaceTransforms( InstancePath, PointInstanceValues, AllTransforms, ObjectTransforms );
                    CreateInstanceInputField( AttributeObject, ObjectTransforms, InstanceInputFields, NewInstanceInputFields );
                    Success = true;
                }
            }
            if ( !Success )
                return false;
        }
        else
        {
            // We don't support this attribute on other owners.
            return false;
        }
    }
    else
    {
        // This is a standard object type instancer.

        // Locate all geo objects requiring instancing (can be multiple if geo / part / object split took place).
        TArray< FHoudiniGeoPartObject > ObjectsToInstance;
        if( UHoudiniAssetComponent* Comp = GetHoudiniAssetComponent() )
        {
            Comp->LocateStaticMeshes( ObjectToInstanceId, ObjectsToInstance );
        }

        // Process each existing detected object that needs to be instanced.
        for ( int32 GeoIdx = 0; GeoIdx < ObjectsToInstance.Num(); ++GeoIdx )
        {
            FHoudiniGeoPartObject & ItemHoudiniGeoPartObject = ObjectsToInstance[ GeoIdx ];

            // Change the transform of the part being instanced to match the instancer
            ItemHoudiniGeoPartObject.TransformMatrix = HoudiniGeoPartObject.TransformMatrix;

            // Locate or create an input field.
            CreateInstanceInputField(
                ItemHoudiniGeoPartObject, AllTransforms, InstanceInputFields, NewInstanceInputFields );
        }
    }

    // Sort and store new fields.
    NewInstanceInputFields.Sort( FHoudiniAssetInstanceInputFieldSortPredicate() );
    CleanInstanceInputFields( InstanceInputFields );
    InstanceInputFields = NewInstanceInputFields;

    return true;
}

UHoudiniAssetInstanceInputField *
UHoudiniAssetInstanceInput::LocateInputField(
    const FHoudiniGeoPartObject & GeoPartObject)
{
    UHoudiniAssetInstanceInputField * FoundHoudiniAssetInstanceInputField = nullptr;
    for ( int32 FieldIdx = 0; FieldIdx < InstanceInputFields.Num(); ++FieldIdx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ FieldIdx ];

        if ( HoudiniAssetInstanceInputField->GetHoudiniGeoPartObject().GetNodePath() == GeoPartObject.GetNodePath() )
        {
            FoundHoudiniAssetInstanceInputField = HoudiniAssetInstanceInputField;
            break;
        }
    }

    return FoundHoudiniAssetInstanceInputField;
}

void
UHoudiniAssetInstanceInput::CleanInstanceInputFields( TArray< UHoudiniAssetInstanceInputField * > & InInstanceInputFields )
{
    UHoudiniAssetInstanceInputField * FoundHoudiniAssetInstanceInputField = nullptr;
    for ( int32 FieldIdx = 0; FieldIdx < InstanceInputFields.Num(); ++FieldIdx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ FieldIdx ];
        if ( HoudiniAssetInstanceInputField)
            HoudiniAssetInstanceInputField->ConditionalBeginDestroy();
    }

    InInstanceInputFields.Empty();
}

UHoudiniAssetComponent*
UHoudiniAssetInstanceInput::GetHoudiniAssetComponent()
{
    return Cast<UHoudiniAssetComponent>( PrimaryObject );
}

const UHoudiniAssetComponent*
UHoudiniAssetInstanceInput::GetHoudiniAssetComponent() const
{
    return Cast<const UHoudiniAssetComponent>( PrimaryObject );
}

HAPI_NodeId
UHoudiniAssetInstanceInput::GetAssetId() const
{
    if( const UHoudiniAssetComponent* Comp = GetHoudiniAssetComponent() )
        return Comp->GetAssetId();
    return -1;
}

void
UHoudiniAssetInstanceInput::CreateInstanceInputField(
    const FHoudiniGeoPartObject & InHoudiniGeoPartObject,
    const TArray< FTransform > & ObjectTransforms,
    const TArray< UHoudiniAssetInstanceInputField * > & OldInstanceInputFields,
    TArray<UHoudiniAssetInstanceInputField * > & NewInstanceInputFields)
{
    UHoudiniAssetComponent* Comp = GetHoudiniAssetComponent();
    UStaticMesh * StaticMesh = Comp ? Comp->LocateStaticMesh( InHoudiniGeoPartObject, false ) : nullptr;

    // Locate static mesh for this geo part.  
    if ( StaticMesh )
    {
        // Locate corresponding input field.
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField =
            LocateInputField( InHoudiniGeoPartObject );

        if ( !HoudiniAssetInstanceInputField )
        {
            // Input field does not exist, we need to create it.
            HoudiniAssetInstanceInputField = UHoudiniAssetInstanceInputField::Create(
                PrimaryObject, this, InHoudiniGeoPartObject );

            // Assign original and static mesh.
            HoudiniAssetInstanceInputField->OriginalObject = StaticMesh;
            HoudiniAssetInstanceInputField->AddInstanceVariation( StaticMesh, 0 );
        }
        else
        {
            // refresh the geo part
            HoudiniAssetInstanceInputField->SetGeoPartObject( InHoudiniGeoPartObject );

            // Remove item from old list.
            InstanceInputFields.RemoveSingleSwap( HoudiniAssetInstanceInputField, false );

            TArray< int > MatchingIndices;

            HoudiniAssetInstanceInputField->FindObjectIndices(
                HoudiniAssetInstanceInputField->GetOriginalObject(),
                MatchingIndices );

            for ( int Idx = 0; Idx < MatchingIndices.Num(); Idx++ )
            {
                int ReplacementIndex = MatchingIndices[ Idx ];
                HoudiniAssetInstanceInputField->ReplaceInstanceVariation( StaticMesh, ReplacementIndex );
            }

            HoudiniAssetInstanceInputField->OriginalObject = StaticMesh;
        }

        // Set transforms for this input.
        HoudiniAssetInstanceInputField->SetInstanceTransforms( ObjectTransforms );
        HoudiniAssetInstanceInputField->UpdateInstanceUPropertyAttributes();

        // Add field to list of fields.
        NewInstanceInputFields.Add( HoudiniAssetInstanceInputField );
    }
    else if ( InHoudiniGeoPartObject.IsPackedPrimitiveInstancer() )
    {
        HAPI_Result Result = HAPI_RESULT_SUCCESS;
        // We seem to be instancing a PP instancer, we need to get the transforms 

        HAPI_PartInfo PartInfo;
        HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetPartInfo(
            FHoudiniEngine::Get().GetSession(), InHoudiniGeoPartObject.GeoId, InHoudiniGeoPartObject.PartId,
            &PartInfo ) );

        // Get transforms for each instance
        TArray<HAPI_Transform> InstancerPartTransforms;
        InstancerPartTransforms.SetNumZeroed( PartInfo.instanceCount );
        HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetInstancerPartTransforms(
            FHoudiniEngine::Get().GetSession(), InHoudiniGeoPartObject.GeoId, PartInfo.id,
            HAPI_RSTORDER_DEFAULT, InstancerPartTransforms.GetData(), 0, PartInfo.instanceCount ) );

        // Get the part ids for parts being instanced
        TArray<HAPI_PartId> InstancedPartIds;
        InstancedPartIds.SetNumZeroed( PartInfo.instancedPartCount );
        HOUDINI_CHECK_ERROR( &Result, FHoudiniApi::GetInstancedPartIds(
            FHoudiniEngine::Get().GetSession(), InHoudiniGeoPartObject.GeoId, PartInfo.id,
            InstancedPartIds.GetData(), 0, PartInfo.instancedPartCount ) );

        for ( auto InstancedPartId : InstancedPartIds )
        {
            HAPI_PartInfo InstancedPartInfo;
            HOUDINI_CHECK_ERROR( &Result,
                FHoudiniApi::GetPartInfo(
                    FHoudiniEngine::Get().GetSession(), InHoudiniGeoPartObject.GeoId, InstancedPartId,
                    &InstancedPartInfo ) );

            TArray<FTransform> PPObjectTransforms;
            PPObjectTransforms.SetNumUninitialized( InstancerPartTransforms.Num() );
            for ( int32 InstanceIdx = 0; InstanceIdx < InstancerPartTransforms.Num(); ++InstanceIdx )
            {
                const auto& InstanceTransform = InstancerPartTransforms[InstanceIdx];
                FHoudiniEngineUtils::TranslateHapiTransform( InstanceTransform, PPObjectTransforms[InstanceIdx] );
            }

            // Create this instanced input field for this instanced part
            
            // find static mesh for this instancer
            FHoudiniGeoPartObject TempInstancedPart( InHoudiniGeoPartObject.AssetId, InHoudiniGeoPartObject.ObjectId, InHoudiniGeoPartObject.GeoId, InstancedPartId );
            if ( UStaticMesh* FoundStaticMesh = Comp->LocateStaticMesh( TempInstancedPart, false ) )
            {
                // Build the list of transforms for this instancer
                TArray< FTransform > AllTransforms;
                AllTransforms.Empty( PPObjectTransforms.Num() * ObjectTransforms.Num() );
                for ( const FTransform& ObjectTransform : ObjectTransforms )
                {
                    for ( const FTransform& PPTransform : PPObjectTransforms )
                    {
                        AllTransforms.Add( PPTransform * ObjectTransform );
                    }
                }

                CreateInstanceInputField( FoundStaticMesh, AllTransforms, InstanceInputFields, NewInstanceInputFields );
            }
            else
            {
                HOUDINI_LOG_WARNING(
                    TEXT( "CreateInstanceInputField for Packed Primitive: Could not find static mesh for object [%d %s], geo %d, part %d]" ), InHoudiniGeoPartObject.ObjectId, *InHoudiniGeoPartObject.ObjectName, InHoudiniGeoPartObject.GeoId, InstancedPartId );
            }
        }
    }
    else
    {
        HOUDINI_LOG_WARNING(
            TEXT( "CreateInstanceInputField: Could not find static mesh for object [%d %s], geo %d, part %d]" ), InHoudiniGeoPartObject.ObjectId, *InHoudiniGeoPartObject.ObjectName, InHoudiniGeoPartObject.GeoId, InHoudiniGeoPartObject.PartId );
    }
}

void
UHoudiniAssetInstanceInput::CreateInstanceInputField(
    UObject * InstancedObject,
    const TArray< FTransform > & ObjectTransforms,
    const TArray< UHoudiniAssetInstanceInputField * > & OldInstanceInputFields,
    TArray< UHoudiniAssetInstanceInputField * > & NewInstanceInputFields )
{
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = nullptr;

    // Locate all fields which have this static mesh set as original mesh.
    TArray< UHoudiniAssetInstanceInputField * > CandidateFields = InstanceInputFields;
    CandidateFields.FilterByPredicate( [&]( const UHoudiniAssetInstanceInputField* Field ) {
        return Field->GetOriginalObject() == InstancedObject;
    } );

    if ( CandidateFields.Num() > 0 )
    {
        HoudiniAssetInstanceInputField = CandidateFields[ 0 ];
        InstanceInputFields.RemoveSingleSwap( HoudiniAssetInstanceInputField, false );

        TArray< int32 > MatchingIndices;

        HoudiniAssetInstanceInputField->FindObjectIndices(
            HoudiniAssetInstanceInputField->GetOriginalObject(),
            MatchingIndices );

        for ( int32 ReplacementIndex : MatchingIndices )
        {
            HoudiniAssetInstanceInputField->ReplaceInstanceVariation( InstancedObject, ReplacementIndex );
        }

        HoudiniAssetInstanceInputField->OriginalObject = InstancedObject;

        // refresh the geo part
        FHoudiniGeoPartObject RefreshedGeoPart = HoudiniAssetInstanceInputField->GetHoudiniGeoPartObject();
        RefreshedGeoPart.TransformMatrix = HoudiniGeoPartObject.TransformMatrix;
        HoudiniAssetInstanceInputField->SetGeoPartObject( RefreshedGeoPart );
        // Update component transformation.
        HoudiniAssetInstanceInputField->UpdateRelativeTransform();
    }
    else
    {
        // Create a dummy part for this field
        FHoudiniGeoPartObject InstancedPart;
        InstancedPart.TransformMatrix = HoudiniGeoPartObject.TransformMatrix;

        HoudiniAssetInstanceInputField = UHoudiniAssetInstanceInputField::Create(
            PrimaryObject, this, InstancedPart );

        // Assign original and static mesh.
        HoudiniAssetInstanceInputField->OriginalObject = InstancedObject;
        HoudiniAssetInstanceInputField->AddInstanceVariation( InstancedObject, 0 );
    }

    // Set transforms for this input.
    HoudiniAssetInstanceInputField->SetInstanceTransforms( ObjectTransforms );

    // Add field to list of fields.
    NewInstanceInputFields.Add( HoudiniAssetInstanceInputField );
}

void
UHoudiniAssetInstanceInput::RecreateRenderStates()
{
    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ Idx ];
        HoudiniAssetInstanceInputField->RecreateRenderState();
    }
}

void
UHoudiniAssetInstanceInput::RecreatePhysicsStates()
{
    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ Idx ];
        HoudiniAssetInstanceInputField->RecreatePhysicsState();
    }
}

void UHoudiniAssetInstanceInput::SetGeoPartObject( const FHoudiniGeoPartObject& InGeoPartObject )
{
    HoudiniGeoPartObject = InGeoPartObject;
    if ( ObjectToInstanceId == -1 )
    {
        ObjectToInstanceId = InGeoPartObject.HapiObjectGetToInstanceId();
    }
}

bool
UHoudiniAssetInstanceInput::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo)
{
    // This implementation is not a true parameter. This method should not be called.
    check( false );
    return false;
}

#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::OnAddInstanceVariation( UHoudiniAssetInstanceInputField * InstanceInputField, int32 Index )
{
    InstanceInputField->AddInstanceVariation( InstanceInputField->GetInstanceVariation( Index ), Index );

    OnParamStateChanged();
}

void
UHoudiniAssetInstanceInput::OnRemoveInstanceVariation( UHoudiniAssetInstanceInputField * InstanceInputField, int32 Index )
{
    InstanceInputField->RemoveInstanceVariation( Index );

    OnParamStateChanged();
}

FString UHoudiniAssetInstanceInput::GetFieldLabel( int32 FieldIdx, int32 VariationIdx ) const
{
    FString FieldNameText;
    UHoudiniAssetInstanceInputField * Field = InstanceInputFields[ FieldIdx ];
    if ( Flags.bIsPackedPrimitiveInstancer )
    {
        FieldNameText = Field->GetHoudiniGeoPartObject().GetNodePath();
    }
    else if ( Flags.bAttributeInstancerOverride )
    {
        FieldNameText = HoudiniGeoPartObject.GetNodePath() + TEXT( "/Override_" ) + FString::FromInt( FieldIdx );
    }
    else
    {
        // For object-instancers we use the instancer's name as well
        FieldNameText = HoudiniGeoPartObject.GetNodePath() + TEXT( "/" ) + Field->GetHoudiniGeoPartObject().ObjectName;
    }

    if ( Field->InstanceVariationCount() > 1 )
        FieldNameText += FString::Printf( TEXT( " [%d]" ), VariationIdx );

    return FieldNameText;
}

#endif

void
UHoudiniAssetInstanceInput::BeginDestroy()
{
    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
        InstanceInputFields[ Idx ]->ConditionalBeginDestroy();

    InstanceInputFields.Empty();

    Super::BeginDestroy();
}

void
UHoudiniAssetInstanceInput::SetHoudiniAssetComponent( UHoudiniAssetComponent * InComponent )
{
    UHoudiniAssetParameter::SetHoudiniAssetComponent( InComponent );

    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
    {
        InstanceInputFields[ Idx ]->HoudiniAssetComponent = InComponent;
        InstanceInputFields[ Idx ]->HoudiniAssetInstanceInput = this;
    }
}

void
UHoudiniAssetInstanceInput::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << Flags.HoudiniAssetInstanceInputFlagsPacked;
    Ar << HoudiniGeoPartObject;

    Ar << ObjectToInstanceId;
    // Object id is transient
    if ( Ar.IsLoading() && !Ar.IsTransacting() )
        ObjectToInstanceId = -1;

    // Serialize fields.
    Ar << InstanceInputFields;
}

void
UHoudiniAssetInstanceInput::AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniAssetInstanceInput * HoudiniAssetInstanceInput = Cast< UHoudiniAssetInstanceInput >( InThis );
    if ( HoudiniAssetInstanceInput )
    {
        // Add references to all used fields.
        for ( int32 Idx = 0; Idx < HoudiniAssetInstanceInput->InstanceInputFields.Num(); ++Idx )
        {
            UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField =
                HoudiniAssetInstanceInput->InstanceInputFields[ Idx ];

            Collector.AddReferencedObject( HoudiniAssetInstanceInputField, InThis );
        }
    }

    // Call base implementation.
    Super::AddReferencedObjects( InThis, Collector );
}

#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::CloneComponentsAndAttachToActor( AActor * Actor )
{
    USceneComponent * RootComponent = Actor->GetRootComponent();

    TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;

    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ Idx ];

        for ( int32 VariationIdx = 0;
            VariationIdx < HoudiniAssetInstanceInputField->InstanceVariationCount(); VariationIdx++ )
        {
            UInstancedStaticMeshComponent * ISMC =
                Cast<UInstancedStaticMeshComponent>( HoudiniAssetInstanceInputField->GetInstancedComponent( VariationIdx ) );
            UHoudiniMeshSplitInstancerComponent * MSIC =
                Cast<UHoudiniMeshSplitInstancerComponent>( HoudiniAssetInstanceInputField->GetInstancedComponent( VariationIdx ) );

            if ( !ISMC && !MSIC )
            {
                if( UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>( HoudiniAssetInstanceInputField->GetInstancedComponent( VariationIdx )) )
                {
                    if( !IAC->InstancedAsset )
                        continue;

                    UClass* ObjectClass = IAC->InstancedAsset->GetClass();
                    
                    TSubclassOf<AActor> ActorClass;

                    if( ObjectClass->IsChildOf<AActor>() )
                    {
                        ActorClass = ObjectClass;
                    }
                    else if( ObjectClass->IsChildOf<UBlueprint>() )
                    {
                        UBlueprint* BlueprintObj = StaticCast<UBlueprint*>( IAC->InstancedAsset );
                        ActorClass = *BlueprintObj->GeneratedClass;
                    }

                    if( *ActorClass )
                    {
                        for( AActor* InstancedActor : IAC->Instances )
                        {
                            if( InstancedActor )
                            {
                                UChildActorComponent* CAC = NewObject< UChildActorComponent >( Actor, UChildActorComponent::StaticClass(), NAME_None, RF_Public );
                                Actor->AddInstanceComponent( CAC );
                                CAC->SetChildActorClass( ActorClass );
                                CAC->RegisterComponent();
                                CAC->SetWorldTransform( InstancedActor->GetTransform() );
                                CAC->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepWorldTransform );
                            }
                        }
                    }
                    else if( ObjectClass->IsChildOf<UParticleSystem>() )
                    {
                        for( AActor* InstancedActor : IAC->Instances )
                        {
                            if( InstancedActor )
                            {
                                UParticleSystemComponent* PSC = NewObject< UParticleSystemComponent >( Actor, UParticleSystemComponent::StaticClass(), NAME_None, RF_Public );
                                Actor->AddInstanceComponent( PSC );
                                PSC->SetTemplate( StaticCast<UParticleSystem*>( IAC->InstancedAsset ) );
                                PSC->RegisterComponent();
                                PSC->SetWorldTransform( InstancedActor->GetTransform() );
                                PSC->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepWorldTransform );
                            }
                        }
                    }
                    else if( ObjectClass->IsChildOf<USoundBase>() )
                    {
                        for( AActor* InstancedActor : IAC->Instances )
                        {
                            if( InstancedActor )
                            {
                                UAudioComponent* AC = NewObject< UAudioComponent >( Actor, UAudioComponent::StaticClass(), NAME_None, RF_Public );
                                Actor->AddInstanceComponent( AC );
                                AC->SetSound( StaticCast<USoundBase*>( IAC->InstancedAsset ) );
                                AC->RegisterComponent();
                                AC->SetWorldTransform( InstancedActor->GetTransform() );
                                AC->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepWorldTransform );
                            }
                        }
                    }
                    else
                    {
                        // Oh no, the asset is not something we know.  We will need to handle each asset type case by case.
                        // for example we could create a bunch of ParticleSystemComponent if given an emitter asset
                        HOUDINI_LOG_ERROR( TEXT( "Can not bake instanced actor component for asset type %s" ), *ObjectClass->GetName() );
                    }
                }
                continue;
            }

            // If original static mesh is used, then we need to bake it (once) and use that one instead.
            UStaticMesh* OutStaticMesh = Cast<UStaticMesh>( HoudiniAssetInstanceInputField->GetInstanceVariation( VariationIdx ) );
            if ( HoudiniAssetInstanceInputField->IsOriginalObjectUsed( VariationIdx ) )
            {
                if( UStaticMesh* OriginalSM = Cast<UStaticMesh>(HoudiniAssetInstanceInputField->GetOriginalObject() ) )
                {
                    auto Comp = GetHoudiniAssetComponent();
                    check( Comp );
                    auto&& ItemGeoPartObject = Comp->LocateGeoPartObject( OutStaticMesh );
                    FHoudiniEngineBakeUtils::CheckedBakeStaticMesh( Comp, OriginalToBakedMesh, ItemGeoPartObject, OriginalSM );
                    OutStaticMesh = OriginalToBakedMesh[ OutStaticMesh ];
                }
            }

            if( ISMC )
            {
                // Do we need a Hierarchical ISMC ?
                UHierarchicalInstancedStaticMeshComponent * HISMC =
                    Cast<UHierarchicalInstancedStaticMeshComponent>( HoudiniAssetInstanceInputField->GetInstancedComponent( VariationIdx ) );

                UInstancedStaticMeshComponent* DuplicatedComponent = nullptr;
                if ( HISMC )
                    DuplicatedComponent = NewObject< UHierarchicalInstancedStaticMeshComponent >(
                        Actor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Public );
                else
                    DuplicatedComponent = NewObject< UInstancedStaticMeshComponent >(
                        Actor, UInstancedStaticMeshComponent::StaticClass(), NAME_None, RF_Public );

                Actor->AddInstanceComponent( DuplicatedComponent );
                DuplicatedComponent->SetStaticMesh( OutStaticMesh );

                // Reapply the uproperties modified by attributes on the duplicated component
                FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject( DuplicatedComponent, HoudiniGeoPartObject );

                // Set component instances.
                UHoudiniInstancedActorComponent::UpdateInstancerComponentInstances(
                    DuplicatedComponent,
                    HoudiniAssetInstanceInputField->GetInstancedTransforms( VariationIdx ),
                    HoudiniAssetInstanceInputField->GetInstancedColors( VariationIdx ),
                    HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx ),
                    HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx ) );

                // Copy visibility.
                DuplicatedComponent->SetVisibility( ISMC->IsVisible() );

                DuplicatedComponent->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepRelativeTransform );
                DuplicatedComponent->RegisterComponent();
                DuplicatedComponent->GetBodyInstance()->bAutoWeld = false;
            }
            else if ( MSIC )
            {
                for( UStaticMeshComponent* OtherSMC : MSIC->GetInstances() )
                {
                    FString CompName = OtherSMC->GetName();
                    CompName.ReplaceInline( TEXT("StaticMeshComponent"), TEXT("StaticMesh") );
                    if( UStaticMeshComponent* NewSMC = DuplicateObject< UStaticMeshComponent >( OtherSMC, Actor, *CompName ) )
                    {
                        NewSMC->SetupAttachment( RootComponent );
                        NewSMC->SetStaticMesh( OutStaticMesh );
                        Actor->AddInstanceComponent( NewSMC );
                        NewSMC->SetWorldTransform( OtherSMC->GetComponentTransform() );
                        NewSMC->RegisterComponent();
                    }
                }
            }
        }
    }
}

#endif

void
UHoudiniAssetInstanceInput::GetPathInstaceTransforms(
    const FString & ObjectInstancePath,
    const TArray< FString > & PointInstanceValues, const TArray< FTransform > & Transforms,
    TArray< FTransform > & OutTransforms)
{
    OutTransforms.Empty();

    for ( int32 Idx = 0; Idx < PointInstanceValues.Num(); ++Idx )
    {
        if ( ObjectInstancePath.Equals( PointInstanceValues[ Idx ] ) )
            OutTransforms.Add( Transforms[ Idx ] );
    }
}

#if WITH_EDITOR

void
UHoudiniAssetInstanceInput::OnStaticMeshDropped(
    UObject * InObject,
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
    int32 Idx, int32 VariationIdx )
{
    ULevel* CurrentLevel = GWorld->GetCurrentLevel();
    ULevel* MyLevel = GetHoudiniAssetComponent()->GetOwner()->GetLevel();

    if( MyLevel != CurrentLevel )
    {
        HOUDINI_LOG_ERROR( TEXT( "%s: Could not spawn instanced actor because the actor's level is not the Current Level." ), *GetHoudiniAssetComponent()->GetOwner()->GetName() );
        FHoudiniEngineUtils::CreateSlateNotification( TEXT( "Could not spawn instanced actor because the actor's level is not the Current Level." ) );

        return;
    }

    UObject * UsedObj = HoudiniAssetInstanceInputField->GetInstanceVariation( VariationIdx );
    if ( InObject && UsedObj != InObject )
    {
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
            PrimaryObject );
        HoudiniAssetInstanceInputField->Modify();

        HoudiniAssetInstanceInputField->ReplaceInstanceVariation( InObject, VariationIdx );

        OnParamStateChanged();
    }
}

FReply
UHoudiniAssetInstanceInput::OnThumbnailDoubleClick(
    const FGeometry & InMyGeometry, const FPointerEvent & InMouseEvent, UObject * Object )
{
    if ( Object && GEditor )
        GEditor->EditObject( Object );

    return FReply::Handled();
}

void UHoudiniAssetInstanceInput::OnInstancedObjectBrowse( UObject* InstancedObject )
{
    if ( GEditor )
    {
        TArray< UObject * > Objects;
        Objects.Add( InstancedObject );
        GEditor->SyncBrowserToObjects( Objects );
    }
}

FReply
UHoudiniAssetInstanceInput::OnResetStaticMeshClicked(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
    int32 Idx, int32 VariationIdx )
{
    UObject * Obj = HoudiniAssetInstanceInputField->GetOriginalObject();
    OnStaticMeshDropped( Obj, HoudiniAssetInstanceInputField, Idx, VariationIdx );

    return FReply::Handled();
}

void
UHoudiniAssetInstanceInput::CloseStaticMeshComboButton(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField,
    int32 Idx, int32 VariationIdx )
{
    // Do nothing.
}

void
UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton(
    bool bOpened, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 Idx, int32 VariationIdx )
{
    if ( !bOpened )
    {
        // If combo button has been closed, update the UI.
        OnParamStateChanged();
    }
}

TOptional< float >
UHoudiniAssetInstanceInput::GetRotationRoll(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx ) const
{
    const FRotator & Rotator = HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx );
    return Rotator.Roll;
}

TOptional< float >
UHoudiniAssetInstanceInput::GetRotationPitch(
    UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField, int32 VariationIdx ) const
{
    const FRotator & Rotator = HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx );
    return Rotator.Pitch;
}

TOptional< float >
UHoudiniAssetInstanceInput::GetRotationYaw(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx ) const
{
    const FRotator& Rotator = HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx );
    return Rotator.Yaw;
}

void
UHoudiniAssetInstanceInput::SetRotationRoll(
    float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx );
    Rotator.Roll = Value;
    HoudiniAssetInstanceInputField->SetRotationOffset( Rotator, VariationIdx );
    HoudiniAssetInstanceInputField->UpdateInstanceTransforms( false );
}

void
UHoudiniAssetInstanceInput::SetRotationPitch(
    float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx );
    Rotator.Pitch = Value;
    HoudiniAssetInstanceInputField->SetRotationOffset( Rotator, VariationIdx );
    HoudiniAssetInstanceInputField->UpdateInstanceTransforms( false );
}

void
UHoudiniAssetInstanceInput::SetRotationYaw(
    float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    FRotator Rotator = HoudiniAssetInstanceInputField->GetRotationOffset( VariationIdx );
    Rotator.Yaw = Value;
    HoudiniAssetInstanceInputField->SetRotationOffset( Rotator, VariationIdx );
    HoudiniAssetInstanceInputField->UpdateInstanceTransforms( false );
}

TOptional< float >
UHoudiniAssetInstanceInput::GetScaleX(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx ) const
{
    const FVector & Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx );
    return Scale3D.X;
}

TOptional< float >
UHoudiniAssetInstanceInput::GetScaleY(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx ) const
{
    const FVector & Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx );
    return Scale3D.Y;
}

TOptional< float >
UHoudiniAssetInstanceInput::GetScaleZ(
    UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx ) const
{
    const FVector & Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx );
    return Scale3D.Z;
}

void
UHoudiniAssetInstanceInput::SetScaleX(
    float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx );
    Scale3D.X = Value;

    if ( HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly( VariationIdx ) )
    {
        Scale3D.Y = Value;
        Scale3D.Z = Value;
    }

    HoudiniAssetInstanceInputField->SetScaleOffset( Scale3D, VariationIdx );
    HoudiniAssetInstanceInputField->UpdateInstanceTransforms( false );
}

void
UHoudiniAssetInstanceInput::SetScaleY(
    float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx)
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx );
    Scale3D.Y = Value;

    if ( HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly( VariationIdx ) )
    {
        Scale3D.X = Value;
        Scale3D.Z = Value;
    }

    HoudiniAssetInstanceInputField->SetScaleOffset( Scale3D, VariationIdx );
    HoudiniAssetInstanceInputField->UpdateInstanceTransforms( false );
}

void
UHoudiniAssetInstanceInput::SetScaleZ(
    float Value, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx)
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    FVector Scale3D = HoudiniAssetInstanceInputField->GetScaleOffset( VariationIdx );
    Scale3D.Z = Value;

    if ( HoudiniAssetInstanceInputField->AreOffsetsScaledLinearly( VariationIdx ) )
    {
        Scale3D.Y = Value;
        Scale3D.X = Value;
    }

    HoudiniAssetInstanceInputField->SetScaleOffset( Scale3D, VariationIdx );
    HoudiniAssetInstanceInputField->UpdateInstanceTransforms( false );
}

void
UHoudiniAssetInstanceInput::CheckStateChanged(
    bool IsChecked, UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField, int32 VariationIdx )
{
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniInstanceInputChange", "Houdini Instance Input Change" ),
        PrimaryObject );
    HoudiniAssetInstanceInputField->Modify();

    HoudiniAssetInstanceInputField->SetLinearOffsetScale( IsChecked, VariationIdx );
}

#endif

bool
UHoudiniAssetInstanceInput::CollectAllInstancedStaticMeshComponents(
    TArray< UInstancedStaticMeshComponent * > & Components, const UStaticMesh * StaticMesh )
{
    bool bCollected = false;
    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ Idx ];
        if ( HoudiniAssetInstanceInputField )
        {
            UStaticMesh * OriginalStaticMesh = Cast< UStaticMesh >( HoudiniAssetInstanceInputField->GetOriginalObject() );
            if ( OriginalStaticMesh == StaticMesh )
            {
                for ( int32 IdxMesh = 0; IdxMesh < HoudiniAssetInstanceInputField->InstancedObjects.Num(); ++IdxMesh )
                {
                    UStaticMesh * UsedStaticMesh = Cast< UStaticMesh >( HoudiniAssetInstanceInputField->InstancedObjects[ IdxMesh ] );
                    if ( UsedStaticMesh == StaticMesh )
                    {
                        UInstancedStaticMeshComponent* ISMC = Cast< UInstancedStaticMeshComponent >( HoudiniAssetInstanceInputField->InstancerComponents[ IdxMesh ] );
                        Components.Add( ISMC );
                        bCollected = true;
                    }
                }
            }
        }
    }

    return bCollected;
}

bool
UHoudiniAssetInstanceInput::GetMaterialReplacementMeshes(
    UMaterialInterface * Material,
    TMap< UStaticMesh *, int32 > & MaterialReplacementsMap )
{
    bool bResult = false;

    for ( int32 Idx = 0; Idx < InstanceInputFields.Num(); ++Idx )
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InstanceInputFields[ Idx ];
        if ( HoudiniAssetInstanceInputField )
            bResult |= HoudiniAssetInstanceInputField->GetMaterialReplacementMeshes( Material, MaterialReplacementsMap );
    }

    return bResult;
}

#undef LOCTEXT_NAMESPACE