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


UHoudiniAssetThumbnailRenderer::UHoudiniAssetThumbnailRenderer(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP),
	ThumbnailScene(nullptr)
{
	
}


void
UHoudiniAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(Object);
	if(HoudiniAsset && !HoudiniAsset->IsPendingKill())
	{
		// If we do not have a scene, create one.
		if(!ThumbnailScene)
		{
			ThumbnailScene = new FHoudiniAssetThumbnailScene();
		}

		ThumbnailScene->SetHoudiniAsset(HoudiniAsset);
		ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);
		
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.LOD = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);


		ThumbnailScene->SetHoudiniAsset(nullptr);
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
