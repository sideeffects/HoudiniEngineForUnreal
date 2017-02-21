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
*      Chris Grebeldinger
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once
#include "ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"

#include "HoudiniAttributeDataComponent.generated.h"

UENUM()
enum EHoudiniVertexAttributeDataType
{
    VADT_Float UMETA( DisplayName = "Float" ),
    VADT_Int32 UMETA( DisplayName = "Integer" ),
    VADT_Bool UMETA( DisplayName = "Boolean" )
};

struct FHoudiniPointAttributeData
{
    FHoudiniPointAttributeData( const FString& InAttrName, UStaticMeshComponent* InComponent, EHoudiniVertexAttributeDataType InDataType, int32 InCount, int32 InTupleSize )
    : AttrName( InAttrName )
    , Component ( InComponent )
    , DataType( InDataType )
    , Count ( InCount )
    , TupleSize ( InTupleSize )
    {
        switch ( InDataType )
        {
            case VADT_Float:
                FloatData.SetNumZeroed( Count * TupleSize );
                break;
            case VADT_Int32:
            case VADT_Bool:
                IntData.SetNumZeroed( Count * TupleSize );
                break;
            default:
                checkNoEntry();
        }
    }
    FString AttrName;
    TWeakObjectPtr<UStaticMeshComponent> Component;
    EHoudiniVertexAttributeDataType DataType;
    int32 Count;
    int32 TupleSize;

    // Only one of the following are used
    TArray<float> FloatData;
    TArray<int32> IntData;
};

UCLASS( config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniAttributeDataComponent : public UActorComponent
{
    GENERATED_UCLASS_BODY()

    virtual ~UHoudiniAttributeDataComponent();

public:
    void SetAttributeData( FHoudiniPointAttributeData&& InVertexAttributeData );
    FHoudiniPointAttributeData* GetAttributeData( class UStaticMeshComponent* StaticMeshComponent );

    /** UObject methods. **/
    void Serialize( FArchive & Ar ) override;

    /** Upload all data for the given mesh to the specified geo node */
    bool Upload( HAPI_NodeId GeoNodeId, class UStaticMeshComponent* StaticMeshComponent ) const;
private:
    TArray< FHoudiniPointAttributeData > VertexAttributeData;
};
