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


void
FHoudiniMeshVertexBuffer::InitRHI()
{
	if(Vertices.Num() <= 0)
	{
		return;
	}

	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex), BUF_Static, CreateInfo);

	// Copy the vertex data into the vertex buffer.
	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
	FMemory::Memcpy(VertexBufferData, Vertices.GetTypedData(), Vertices.Num() * sizeof(FDynamicMeshVertex));
	RHIUnlockVertexBuffer(VertexBufferRHI);
}
