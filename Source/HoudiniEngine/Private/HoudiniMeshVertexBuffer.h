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

#pragma once

namespace EHoudiniMeshVertexBufferSemantic
{
	enum Type
	{
		Position,
		Normal,
		Color,
		TextureCoordinate0,
		TextureCoordinate1,
		TextureCoordinate2,
		TextureCoordinate3,
		TextureCoordinate4,
		TextureCoordinate5,
		TextureCoordinate6,
		TextureCoordinate7,
		PackedTangentX,
		PackedTangentZ
	};
}

class FHoudiniMeshVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. **/
	FHoudiniMeshVertexBuffer(const uint8* InDataBytes, uint32 InDataBytesCount, EVertexElementType InDataType,
							 uint32 InDataTupleSize, EHoudiniMeshVertexBufferSemantic::Type InSemantic);

	/** Destructor. **/
	virtual ~FHoudiniMeshVertexBuffer();

public:

	/** Return UV channel index used by this stream. **/
	int32 GetTextureCoordinateChannelIndex() const;

public: /** FRenderResource methods. **/

	virtual void InitRHI() override;

public:

	/** Buffer containing raw data. **/
	uint8* DataBytes;

	/** Size of raw data in bytes. **/
	uint32 DataBytesCount;

	/** Type of each entry. **/
	EVertexElementType DataType;

	/** Number of entries in a tuple. **/
	uint32 DataTupleSize;

	/** Used by UV streams to keep UV channel index. **/
	int32 UVChannelIndex;

	/** Semantic meaning of this vertrex stream. **/
	EHoudiniMeshVertexBufferSemantic::Type Semantic;
};
