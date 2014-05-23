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


FHoudiniAssetThumbnailScene::FHoudiniAssetThumbnailScene()
	: FThumbnailPreviewScene()
{
	bForceAllUsedMipsResident = false;

	// Create preview actor
	// checked
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bNoCollisionFail = true;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;

	// Create spawn actor.
	PreviewHoudiniAssetActor = GetWorld()->SpawnActor<AHoudiniAssetActor>(SpawnInfo);

	PreviewHoudiniAssetActor->HoudiniAssetComponent->SetMobility(EComponentMobility::Movable);
	PreviewHoudiniAssetActor->SetActorEnableCollision(false);
}


void 
FHoudiniAssetThumbnailScene::SetHoudiniAsset(UHoudiniAsset* HoudiniAsset)
{
	PreviewHoudiniAssetActor->HoudiniAssetComponent->SetHoudiniAsset(HoudiniAsset);

	if(HoudiniAsset)
	{
		FTransform MeshTransform = FTransform::Identity;

		PreviewHoudiniAssetActor->SetActorLocation(FVector(0, 0, 0), false);
		PreviewHoudiniAssetActor->HoudiniAssetComponent->UpdateBounds();

		// Center the mesh at the world origin then offset to put it on top of the plane
		const float BoundsZOffset = GetBoundsZOffset(PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds);
		PreviewHoudiniAssetActor->SetActorLocation(-PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds.Origin + FVector(0, 0, BoundsZOffset), false);
		PreviewHoudiniAssetActor->HoudiniAssetComponent->RecreateRenderState_Concurrent();
	}
}


void 
FHoudiniAssetThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(PreviewHoudiniAssetActor);
	check(PreviewHoudiniAssetActor->HoudiniAssetComponent);
	check(PreviewHoudiniAssetActor->HoudiniAssetComponent->HoudiniAsset);

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;

	// Add extra size to view slightly outside of the sphere to compensate for perspective
	const float HalfMeshSize = PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds.SphereRadius * 1.15;
	const float BoundsZOffset = GetBoundsZOffset(PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(PreviewHoudiniAssetActor->HoudiniAssetComponent->HoudiniAsset->ThumbnailInfo);
	if(ThumbnailInfo)
	{
		if(TargetDistance + ThumbnailInfo->OrbitZoom < 0)
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}
