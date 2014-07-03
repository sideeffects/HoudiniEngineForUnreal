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

#include "HoudiniEnginePrivatePCH.h"


FHoudiniMeshVertexFactory::FHoudiniMeshVertexFactory()
{

}


void
FHoudiniMeshVertexFactory::Init(const FHoudiniMeshVertexBuffer* VertexBuffer)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitHoudiniMeshVertexFactory,
		FHoudiniMeshVertexFactory*,
		VertexFactory,
		this,
		const FHoudiniMeshVertexBuffer*,
		VertexBuffer,
		VertexBuffer,
		{
			// Initialize the vertex factory's stream components.
			DataType NewData;

			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
			NewData.TextureCoordinates.Add(FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2));
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
			NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Color, VET_Color);

			VertexFactory->SetData(NewData);
		});
}
