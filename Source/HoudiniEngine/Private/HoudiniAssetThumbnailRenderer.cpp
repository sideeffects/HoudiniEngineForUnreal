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
	/*
	static UTexture2D* GridTexture = NULL;
	if (GridTexture == NULL)
	{
		GridTexture = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EngineMaterials/DefaultWhiteGrid.DefaultWhiteGrid"), NULL, LOAD_None, NULL);
	}

	const bool bAlphaBlend = false;

	Canvas->DrawTile(
		(float)X,
		(float)Y,
		(float)Width,
		(float)Height,
		0.0f,
		0.0f,
		4.0f,
		4.0f,
		FLinearColor(1.0f, 0.647f, 0.0f),
		GridTexture->Resource,
		bAlphaBlend);
	*/

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
			.SetWorldTimes(GCurrentTime - GStartTime, GDeltaTime, GCurrentTime - GStartTime));

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
