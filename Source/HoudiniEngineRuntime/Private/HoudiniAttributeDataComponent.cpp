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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAttributeDataComponent.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniPluginSerializationVersion.h"

UHoudiniAttributeDataComponent::UHoudiniAttributeDataComponent( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
{
}

UHoudiniAttributeDataComponent::~UHoudiniAttributeDataComponent()
{}

void 
UHoudiniAttributeDataComponent::SetAttributeData( FHoudiniPointAttributeData&& InVertexAttributeData )
{
    VertexAttributeData.Add( InVertexAttributeData );
}

FHoudiniPointAttributeData* 
UHoudiniAttributeDataComponent::GetAttributeData( class UStaticMeshComponent* StaticMeshComponent )
{
    for ( FHoudiniPointAttributeData& AttributeData : VertexAttributeData )
    {
        if ( AttributeData.Component.IsValid() && AttributeData.Component.Get() == StaticMeshComponent )
        {
            return &AttributeData;
        }
    }
    return nullptr;
}

void
UHoudiniAttributeDataComponent::Serialize( FArchive & Ar )
{
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    int32 Version = VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION;
    Ar << Version;
}

bool 
UHoudiniAttributeDataComponent::Upload( HAPI_NodeId GeoNodeId, class UStaticMeshComponent* StaticMeshComponent ) const
{
    for ( const FHoudiniPointAttributeData& AttributeData : VertexAttributeData )
    {
        if ( AttributeData.Component.IsValid() && AttributeData.Component.Get() == StaticMeshComponent )
        {
            HAPI_AttributeInfo AttributeInfoPoint {};
            AttributeInfoPoint.count = AttributeData.Count;
            AttributeInfoPoint.tupleSize = AttributeData.TupleSize;
            AttributeInfoPoint.exists = true;
            AttributeInfoPoint.owner = HAPI_ATTROWNER_POINT;
            AttributeInfoPoint.originalOwner = HAPI_ATTROWNER_INVALID;

            const char* AttrNameRaw = TCHAR_TO_ANSI( *AttributeData.AttrName );

            switch ( AttributeData.DataType )
            {
                case EHoudiniVertexAttributeDataType::VADT_Bool:
                case EHoudiniVertexAttributeDataType::VADT_Int32:
                {
                    AttributeInfoPoint.storage = HAPI_STORAGETYPE_INT;

                    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(), GeoNodeId,
                        0, AttrNameRaw, &AttributeInfoPoint ), false );

                    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeIntData(
                        FHoudiniEngine::Get().GetSession(),
                        GeoNodeId, 0, AttrNameRaw, &AttributeInfoPoint,
                        AttributeData.IntData.GetData(), 0, AttributeInfoPoint.count ), false );

                    break;
                }
                case EHoudiniVertexAttributeDataType::VADT_Float:
                {
                    AttributeInfoPoint.storage = HAPI_STORAGETYPE_FLOAT;

                    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::AddAttribute(
                        FHoudiniEngine::Get().GetSession(), GeoNodeId,
                        0, AttrNameRaw, &AttributeInfoPoint ), false );

                    HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::SetAttributeFloatData(
                        FHoudiniEngine::Get().GetSession(),
                        GeoNodeId, 0, AttrNameRaw, &AttributeInfoPoint,
                        AttributeData.FloatData.GetData(), 0, AttributeInfoPoint.count ), false );

                    break;
                }
                default:
                    checkNoEntry();
            }
            break;
        }
    }
    return true;
}
