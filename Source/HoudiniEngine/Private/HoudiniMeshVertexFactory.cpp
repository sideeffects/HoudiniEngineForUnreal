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

			// Position component.
			NewData.PositionComponent = FVertexStreamComponent(
			VertexBuffer,
			STRUCT_OFFSET(FHoudiniMeshVertex, Position),
			sizeof(FHoudiniMeshVertex),
			VET_Float3
			);

			// Color component.
			NewData.ColorComponent = FVertexStreamComponent(
			VertexBuffer,
			STRUCT_OFFSET(FHoudiniMeshVertex, Color),
			sizeof(FHoudiniMeshVertex),
			VET_Float3
			);

			// Texture coordinate components.
			{
				NewData.TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffer,
				STRUCT_OFFSET(FHoudiniMeshVertex, TextureCoordinates[0]),
				sizeof(FHoudiniMeshVertex),
				VET_Float2
				));

				// Texture coordinate component, UV1.
				NewData.TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffer,
				STRUCT_OFFSET(FHoudiniMeshVertex, TextureCoordinates[1]),
				sizeof(FHoudiniMeshVertex),
				VET_Float2
				));
			}

			// Tangent components.
			{
				NewData.TangentBasisComponents[0] = FVertexStreamComponent(
				VertexBuffer,
				STRUCT_OFFSET(FHoudiniMeshVertex, PackedTangent[0]),
				sizeof(FHoudiniMeshVertex),
				VET_PackedNormal
				);

				NewData.TangentBasisComponents[1] = FVertexStreamComponent(
				VertexBuffer,
				STRUCT_OFFSET(FHoudiniMeshVertex, PackedTangent[1]),
				sizeof(FHoudiniMeshVertex),
				VET_PackedNormal
				);
			}

			/*
			if(VertexBuffer->CheckUsedField(EHoudiniMeshVertexField::Position))
			{
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FHoudiniMeshVertex, Position, VET_Float3);
			}
			*/

			VertexFactory->SetData(NewData);
		});
}
