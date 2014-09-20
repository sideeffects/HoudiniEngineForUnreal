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


FHoudiniMeshVertexBuffer::FHoudiniMeshVertexBuffer(const uint8* InDataBytes, uint32 InDataBytesCount,
												   EVertexElementType InDataType, uint32 InDataTupleSize,
												   EHoudiniMeshVertexBufferSemantic::Type InSemantic) :
	FVertexBuffer(),
	DataBytes(nullptr),
	DataBytesCount(InDataBytesCount),
	DataType(InDataType),
	DataTupleSize(InDataTupleSize),
	UVChannelIndex(-1),
	Semantic(InSemantic)
{
	if(DataBytesCount)
	{
		// Allocate buffer to store OTL raw data.
		DataBytes = static_cast<uint8*>(FMemory::Malloc(DataBytesCount));

		if(DataBytes)
		{
			// Copy data into a newly allocated buffer.
			FMemory::Memcpy(DataBytes, InDataBytes, DataBytesCount);
		}
	}
}


FHoudiniMeshVertexBuffer::~FHoudiniMeshVertexBuffer()
{
	if(DataBytes)
	{
		FMemory::Free(DataBytes);
		DataBytes = nullptr;
		DataBytesCount = 0;
	}
}


int32
FHoudiniMeshVertexBuffer::GetTextureCoordinateChannelIndex() const
{
	if((Semantic >= EHoudiniMeshVertexBufferSemantic::TextureCoordinate0) && 
			(Semantic <= EHoudiniMeshVertexBufferSemantic::TextureCoordinate7))
	{
		return Semantic - (int32) EHoudiniMeshVertexBufferSemantic::TextureCoordinate0;
	}

	return -1;
}


void
FHoudiniMeshVertexBuffer::InitRHI()
{
	if(!DataBytes)
	{
		return;
	}

	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer(DataBytesCount, BUF_Static, CreateInfo);

	// Copy the vertex data into the vertex buffer.
	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, DataBytesCount, RLM_WriteOnly);
	FMemory::Memcpy(VertexBufferData, DataBytes, DataBytesCount);
	RHIUnlockVertexBuffer(VertexBufferRHI);
}
