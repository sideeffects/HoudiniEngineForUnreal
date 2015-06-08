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

#pragma once
#include "HoudiniAssetThumbnailRenderer.generated.h"


class UObject;
class FCanvas;
class FRenderTarget;
class UHoudiniAsset;
class FHoudiniAssetThumbnailScene;


UCLASS(config=Editor)
class UHoudiniAssetThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

/** ThumbnailRenderer methods. **/
public:

	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget,
		FCanvas* Canvas) override;

/** UObject methods. **/
public:

	virtual void BeginDestroy() override;

private:

	/** Used thumbnail scene. **/
	FHoudiniAssetThumbnailScene* ThumbnailScene;
};
