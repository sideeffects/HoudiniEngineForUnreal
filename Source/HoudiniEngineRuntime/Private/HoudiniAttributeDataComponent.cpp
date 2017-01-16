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
