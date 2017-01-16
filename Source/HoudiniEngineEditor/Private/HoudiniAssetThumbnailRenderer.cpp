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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetThumbnailRenderer.h"
#include "HoudiniAssetThumbnailScene.h"
#include "HoudiniAsset.h"


UHoudiniAssetThumbnailRenderer::UHoudiniAssetThumbnailRenderer( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , ThumbnailScene( nullptr )
{}

void
UHoudiniAssetThumbnailRenderer::Draw(
    UObject * Object, int32 X, int32 Y, uint32 Width, uint32 Height,
    FRenderTarget * RenderTarget, FCanvas * Canvas )
{
    UHoudiniAsset * HoudiniAsset = Cast< UHoudiniAsset >( Object );
    if ( HoudiniAsset && !HoudiniAsset->IsPendingKill() )
    {
        if ( !ThumbnailScene )
            ThumbnailScene = new FHoudiniAssetThumbnailScene();

        ThumbnailScene->SetHoudiniAsset( HoudiniAsset );
        ThumbnailScene->GetScene()->UpdateSpeedTreeWind( 0.0 );

        FSceneViewFamilyContext ViewFamily(
            FSceneViewFamily::ConstructionValues(
                RenderTarget,
                ThumbnailScene->GetScene(), FEngineShowFlags( ESFIM_Game ) )
            .SetWorldTimes( FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime ) );

        ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
        ViewFamily.EngineShowFlags.MotionBlur = 0;
        ViewFamily.EngineShowFlags.LOD = 0;

        ThumbnailScene->GetView( &ViewFamily, X, Y, Width, Height );
        GetRendererModule().BeginRenderingViewFamily( Canvas, &ViewFamily );
    }
}

void
UHoudiniAssetThumbnailRenderer::BeginDestroy()
{
    if ( ThumbnailScene )
    {
        delete ThumbnailScene;
        ThumbnailScene = nullptr;
    }

    Super::BeginDestroy();
}
