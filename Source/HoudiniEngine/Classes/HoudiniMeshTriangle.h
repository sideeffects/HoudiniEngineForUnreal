/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HoudiniMeshTriangle.generated.h"

USTRUCT(BlueprintType)
struct FHoudiniMeshTriangle
{
	GENERATED_USTRUCT_BODY()

	/** Position information. **/
	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector Vertex0;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector Vertex1;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector Vertex2;

	/** Normals information. **/
	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector Normal0;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector Normal1;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector Normal2;

	/** Texture coordinates information. **/
	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector2D TextureCoordinate0;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector2D TextureCoordinate1;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FVector2D TextureCoordinate2;

	/** Per vertex color information. **/
	UPROPERTY(EditAnywhere, Category = Triangle)
	FColor Color0;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FColor Color1;

	UPROPERTY(EditAnywhere, Category = Triangle)
	FColor Color2;
};


FORCEINLINE 
FArchive& 
operator<<(FArchive& Ar, FHoudiniMeshTriangle& triangle)
{
	Ar << triangle.Vertex0; Ar << triangle.Vertex1; Ar << triangle.Vertex2;
	Ar << triangle.Normal0; Ar << triangle.Normal1; Ar << triangle.Normal2;
	Ar << triangle.TextureCoordinate0.X; Ar << triangle.TextureCoordinate0.Y;

	Ar << triangle.Color0.R; Ar << triangle.Color0.G; Ar << triangle.Color0.B; Ar << triangle.Color0.A;
	Ar << triangle.Color1.R; Ar << triangle.Color1.G; Ar << triangle.Color1.B; Ar << triangle.Color1.A;
	Ar << triangle.Color2.R; Ar << triangle.Color2.G; Ar << triangle.Color2.B; Ar << triangle.Color2.A;

	return Ar;
}
