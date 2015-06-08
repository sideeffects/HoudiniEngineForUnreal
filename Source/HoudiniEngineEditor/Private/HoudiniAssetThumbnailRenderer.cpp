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

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetThumbnailRenderer.h"
#include "HoudiniAssetThumbnailScene.h"
#include "HoudiniAsset.h"


UHoudiniAssetThumbnailRenderer::UHoudiniAssetThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	ThumbnailScene(nullptr)
{

}


void
UHoudiniAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
	FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(Object);
	if(HoudiniAsset && !HoudiniAsset->IsPendingKill())
	{
		if(!ThumbnailScene)
		{
			ThumbnailScene = new FHoudiniAssetThumbnailScene();
		}

		ThumbnailScene->SetHoudiniAsset(HoudiniAsset);
		ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget,
			ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.LOD = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
	}
}


void
UHoudiniAssetThumbnailRenderer::BeginDestroy()
{
	if(ThumbnailScene)
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
