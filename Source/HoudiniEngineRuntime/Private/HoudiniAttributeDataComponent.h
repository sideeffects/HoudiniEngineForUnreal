/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
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
