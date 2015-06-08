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


class UHoudiniAsset;
class AHoudiniAssetActor;

class FHoudiniAssetThumbnailScene : public FThumbnailPreviewScene
{
public:

	/** Constructor */
	FHoudiniAssetThumbnailScene();

	/** Sets the houdini asset to use in the next GetView(). **/
	void SetHoudiniAsset(UHoudiniAsset* HoudiniAsset);

/** FThumbnailPreviewScene methods. **/
protected:

	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch,
		float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:

	/** The Houdini asset actor used to display all Houdini asset thumbnails */
	AHoudiniAssetActor* PreviewHoudiniAssetActor;
};
