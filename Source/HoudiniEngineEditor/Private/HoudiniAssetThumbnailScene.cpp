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
    UWorld * world = GetWorld();
    PreviewHoudiniAssetActor = world ? world->SpawnActor< AHoudiniAssetActor >( SpawnInfo ) : nullptr;

    if ( PreviewHoudiniAssetActor && PreviewHoudiniAssetActor->HoudiniAssetComponent )
    {
        PreviewHoudiniAssetActor->HoudiniAssetComponent->SetMobility( EComponentMobility::Movable );
        PreviewHoudiniAssetActor->SetActorEnableCollision( false );
    }
}

void
FHoudiniAssetThumbnailScene::SetHoudiniAsset( UHoudiniAsset * HoudiniAsset )
{
    if ( !HoudiniAsset )
        return;

    if ( !PreviewHoudiniAssetActor || !PreviewHoudiniAssetActor->IsValidLowLevel() )
        return;

    UHoudiniAssetComponent* PreviewHoudiniAssetComponent = PreviewHoudiniAssetActor->GetHoudiniAssetComponent();
    if (!PreviewHoudiniAssetActor->HoudiniAssetComponent || !PreviewHoudiniAssetActor->HoudiniAssetComponent->IsValidLowLevel())
        return;

    if ( PreviewHoudiniAssetComponent->ContainsHoudiniLogoGeometry() )
    {
        float BoundsZOffset = GetBoundsZOffset( PreviewHoudiniAssetComponent->Bounds );

        PreviewHoudiniAssetActor->SetActorLocation( FVector( 0.0f, 0.0f, BoundsZOffset ), false );
        PreviewHoudiniAssetActor->SetActorRotation( FRotator( 0.0f, 175.0f, 0.0f ) );
    }

    PreviewHoudiniAssetComponent->SetHoudiniAsset( HoudiniAsset );
    PreviewHoudiniAssetComponent->UpdateBounds();

    PreviewHoudiniAssetComponent->RecreateRenderState_Concurrent();
}

void
FHoudiniAssetThumbnailScene::GetViewMatrixParameters(
    const float InFOVDegrees, FVector & OutOrigin,
    float & OutOrbitPitch, float & OutOrbitYaw, float & OutOrbitZoom ) const
{
    check( PreviewHoudiniAssetActor );    

    UHoudiniAssetComponent* PreviewHoudiniAssetComponent = PreviewHoudiniAssetActor->GetHoudiniAssetComponent();
    check( PreviewHoudiniAssetComponent );    

    if ( !IsValid() )
        return;

    const float HalfFOVRadians = FMath::DegreesToRadians< float >( InFOVDegrees ) * 0.5f;

    // Add extra size to view slightly outside of the sphere to compensate for perspective
    const float HalfMeshSize = PreviewHoudiniAssetComponent->Bounds.SphereRadius * 1.15f;
    const float BoundsZOffset = GetBoundsZOffset( PreviewHoudiniAssetComponent->Bounds );
    const float TargetDistance = HalfMeshSize / FMath::Tan( HalfFOVRadians );

    UHoudiniAsset * PreviewHoudiniAsset = PreviewHoudiniAssetComponent->GetHoudiniAsset();
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

bool
FHoudiniAssetThumbnailScene::IsValid() const
{
    if (!PreviewHoudiniAssetActor || !PreviewHoudiniAssetActor->IsValidLowLevel())
        return false;

    if ( !GetWorld() )
        return false;

    UHoudiniAssetComponent* PreviewHoudiniAssetComponent = PreviewHoudiniAssetActor->GetHoudiniAssetComponent();
    if ( !PreviewHoudiniAssetComponent || !PreviewHoudiniAssetComponent->IsValidLowLevel() )
        return false;

    return true;
}
