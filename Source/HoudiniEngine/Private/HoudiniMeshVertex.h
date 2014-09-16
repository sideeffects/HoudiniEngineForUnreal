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

namespace EHoudiniMeshVertexField
{
	enum Type
	{
		None,

		Position						= (1 << 0),

		TextureCoordinate0				= (1 << 1),
		TextureCoordinate1				= (1 << 2),
		TextureCoordinate2				= (1 << 3),
		TextureCoordinate3				= (1 << 4),
		TextureCoordinate4				= (1 << 5),
		TextureCoordinate5				= (1 << 6),
		TextureCoordinate6				= (1 << 7),
		TextureCoordinate7				= (1 << 8),

		PackedTangent0					= (1 << 9),
		PackedTangent1					= (1 << 10),

		UnpackedTangent0				= (1 << 11),
		UnpackedTangent1				= (1 << 12),

		Normal							= (1 << 13),
		Color							= (1 << 14)
	};
}

struct FHoudiniMeshVertex
{

public:

	/** Default constructor. **/
	FHoudiniMeshVertex();

public:

	/** Serialize this vertex. **/
	friend FArchive& operator<<(FArchive& Ar, FHoudiniMeshVertex& V);

public:

	/** Set zero normal. **/
	void SetZeroNormal();

	/** Set zero texture coordinates for specified channel. **/
	void SetZeroTextureCoordinates(int32 Channel);

	/** Set zero packed tangents. **/
	void SetZeroPackedTangents();

	/** Set zero color (solid black). **/
	void SetZeroColor();

public:

	/** Position information. **/
	FVector Position;

	/** Texture coordinates information. **/
	FVector2D TextureCoordinates[MAX_STATIC_TEXCOORDS];

	/** Tangent information (unreal specific packing). **/
	FPackedNormal PackedTangent[2];

	/** Unpacked tangent information (tangent and bitangent/binormal). **/
	FVector UnpackedTangent[2];

	/** Normal information. **/
	FVector Normal;

	/** Color information. **/
	FColor Color;
};

FArchive&
operator<<(FArchive& Ar, FHoudiniMeshVertex& Vertex);
