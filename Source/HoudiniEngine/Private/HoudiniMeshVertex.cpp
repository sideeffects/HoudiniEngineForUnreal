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


FArchive& 
operator<<(FArchive& Ar, FHoudiniMeshVertex& Vertex)
{
	Ar << Vertex.Position;
	Ar << Vertex.PackedTangent[0] << Vertex.PackedTangent[1];
	Ar << Vertex.UnpackedTangent[0] << Vertex.UnpackedTangent[1];
	Ar << Vertex.Normal;
	Ar << Vertex.Color;

	for(int32 TexCoordIdx = 0; TexCoordIdx < MAX_STATIC_TEXCOORDS; ++TexCoordIdx)
	{
		Ar << Vertex.TextureCoordinates[TexCoordIdx];
	}

	return Ar;
}


FHoudiniMeshVertex::FHoudiniMeshVertex()
{

}


void
FHoudiniMeshVertex::SetZeroNormal()
{
	Normal = FVector::ZeroVector;
}


void
FHoudiniMeshVertex::SetZeroTextureCoordinates(int32 Channel)
{
	check(Channel >= 0 && Channel < MAX_STATIC_TEXCOORDS);
	TextureCoordinates[Channel] = FVector2D::ZeroVector;
}


void
FHoudiniMeshVertex::SetZeroPackedTangents()
{
	PackedTangent[0] = FPackedNormal::ZeroNormal;
	PackedTangent[1] = FPackedNormal::ZeroNormal;
}


void
FHoudiniMeshVertex::SetZeroColor()
{
	Color = FColor::Black;
	Color.A = 255;
}
