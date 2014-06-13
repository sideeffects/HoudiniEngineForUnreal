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
