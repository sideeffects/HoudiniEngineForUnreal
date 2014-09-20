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
FHoudiniMeshVertexFactory::Init(const TArray<FHoudiniMeshVertexBuffer*>& HoudiniMeshVertexBuffers)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitHoudiniMeshVertexFactory,
		FHoudiniMeshVertexFactory*,
		VertexFactory,
		this,
		const TArray<FHoudiniMeshVertexBuffer*>&,
		HoudiniMeshVertexBuffers,
		HoudiniMeshVertexBuffers,
		{
			// Initialize the vertex factory's stream components.
			DataType NewData;

			for(TArray<FHoudiniMeshVertexBuffer*>::TConstIterator Iter = HoudiniMeshVertexBuffers.CreateConstIterator(); Iter; ++Iter)
			{
				FHoudiniMeshVertexBuffer* HoudiniMeshVertexBuffer = *Iter;

				// Depending on semantic, bind necessary stream.
				switch(HoudiniMeshVertexBuffer->Semantic)
				{
					case EHoudiniMeshVertexBufferSemantic::Position:
					{
						NewData.PositionComponent = FVertexStreamComponent(HoudiniMeshVertexBuffer, 0, sizeof(float) * 3, VET_Float3);
						break;
					}

					case EHoudiniMeshVertexBufferSemantic::Color:
					{
						NewData.ColorComponent = FVertexStreamComponent(HoudiniMeshVertexBuffer, 0, sizeof(float) * 3, VET_Float3);
						break;
					}

					case EHoudiniMeshVertexBufferSemantic::PackedTangentX:
					{
						NewData.TangentBasisComponents[0] = FVertexStreamComponent(HoudiniMeshVertexBuffer, 0, sizeof(uint8) * 4, VET_PackedNormal);
						break;
					}

					case EHoudiniMeshVertexBufferSemantic::PackedTangentZ:
					{
						NewData.TangentBasisComponents[1] = FVertexStreamComponent(HoudiniMeshVertexBuffer, 0, sizeof(uint8) * 4, VET_PackedNormal);
						break;
					}

					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate0:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate1:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate2:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate3:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate4:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate5:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate6:
					case EHoudiniMeshVertexBufferSemantic::TextureCoordinate7:
					{
						NewData.TextureCoordinates.Add(FVertexStreamComponent(HoudiniMeshVertexBuffer, 0, sizeof(float) * 2, VET_Float2));
						break;
					}

					default:
					{
						// We don't know how to handle this semantic - process custom.
						break;
					}
				}
			}

			VertexFactory->SetData(NewData);
		});
}
