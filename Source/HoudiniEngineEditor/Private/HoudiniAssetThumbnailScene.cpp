/*
 * Copyright (c) <2017> Side Effects Software Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

#include "HoudiniApi.h"
#include "HoudiniAssetThumbnailScene.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

FHoudiniAssetThumbnailScene::FHoudiniAssetThumbnailScene()
    : FThumbnailPreviewScene()
{
    bForceAllUsedMipsResident = false;

    // Create preview actor.
    FActorSpawnParameters SpawnInfo;
    SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnInfo.bNoFail = true;

    // Preview actor should be marked as transient.
    SpawnInfo.ObjectFlags = RF_Transient;

    // Create spawn actor.
    PreviewHoudiniAssetActor = GetWorld()->SpawnActor< AHoudiniAssetActor >( SpawnInfo );

    PreviewHoudiniAssetActor->HoudiniAssetComponent->SetMobility( EComponentMobility::Movable );
    PreviewHoudiniAssetActor->SetActorEnableCollision( false );
}

void
FHoudiniAssetThumbnailScene::SetHoudiniAsset( UHoudiniAsset * HoudiniAsset )
{
    if ( HoudiniAsset )
    {
        if ( PreviewHoudiniAssetActor->HoudiniAssetComponent->ContainsHoudiniLogoGeometry() )
        {
            float BoundsZOffset = GetBoundsZOffset( PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds );

            PreviewHoudiniAssetActor->SetActorLocation( FVector( 0.0f, 0.0f, BoundsZOffset ), false );
            PreviewHoudiniAssetActor->SetActorRotation( FRotator( 0.0f, 175.0f, 0.0f ) );
        }

        PreviewHoudiniAssetActor->GetHoudiniAssetComponent()->SetHoudiniAsset( HoudiniAsset );
        PreviewHoudiniAssetActor->HoudiniAssetComponent->UpdateBounds();

        PreviewHoudiniAssetActor->HoudiniAssetComponent->RecreateRenderState_Concurrent();
    }
}

void
FHoudiniAssetThumbnailScene::GetViewMatrixParameters(
    const float InFOVDegrees, FVector & OutOrigin,
    float & OutOrbitPitch, float & OutOrbitYaw, float & OutOrbitZoom ) const
{
    check( PreviewHoudiniAssetActor );
    check( PreviewHoudiniAssetActor->HoudiniAssetComponent );

    const float HalfFOVRadians = FMath::DegreesToRadians< float >( InFOVDegrees ) * 0.5f;

    // Add extra size to view slightly outside of the sphere to compensate for perspective
    const float HalfMeshSize = PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds.SphereRadius * 1.15f;
    const float BoundsZOffset = GetBoundsZOffset( PreviewHoudiniAssetActor->HoudiniAssetComponent->Bounds );
    const float TargetDistance = HalfMeshSize / FMath::Tan( HalfFOVRadians );

    UHoudiniAsset * PreviewHoudiniAsset = PreviewHoudiniAssetActor->GetHoudiniAssetComponent()->GetHoudiniAsset();
    USceneThumbnailInfo * ThumbnailInfo = Cast<USceneThumbnailInfo>( PreviewHoudiniAsset->ThumbnailInfo );

    if ( ThumbnailInfo )
    {
        if ( TargetDistance + ThumbnailInfo->OrbitZoom < 0 )
            ThumbnailInfo->OrbitZoom = -TargetDistance;
    }
    else
    {
        ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject< USceneThumbnailInfo >();
    }

    OutOrigin = FVector( 0, 0, -BoundsZOffset );
    OutOrbitPitch = ThumbnailInfo->OrbitPitch;
    OutOrbitYaw = ThumbnailInfo->OrbitYaw;
    OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}
