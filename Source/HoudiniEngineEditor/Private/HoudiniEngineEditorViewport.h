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

#if 0
#include "PreviewScene.h"

class SDockTab;
class FPreviewScene;
class UHoudiniEngineAsset;
class IHoudiniEngineEditor;
class FEditorViewportClient;
class FHoudiniEngineEditorViewportClient;

class SHoudiniEngineEditorViewport : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SHoudiniEngineEditorViewport){}
	SLATE_ARGUMENT(TWeakPtr<IHoudiniEngineEditor>, HoudiniEngineEditor)
		SLATE_ARGUMENT(UHoudiniEngineAsset*, HoudiniEngineAssetToEdit)
		SLATE_END_ARGS()

public:

	/** Set the parent tab of the viewport for determining visibility. **/
	void SetParentTab(TSharedRef<SDockTab> InParentTab);

public:

	void Construct(const FArguments& InArgs);
	~SHoudiniEngineEditorViewport();

protected: /** SEditorViewport interface. **/

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() = 0;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() = 0;

private:

	/** The parent tab where this viewport resides **/
	TWeakPtr<SDockTab> ParentTab;

	/** Pointer back to the StaticMesh editor tool that owns us. **/
	TWeakPtr<IHoudiniEngineEditor> HoudiniEngineEditor;

	/** Houdini Engine asset which ise being edited. **/
	UHoudiniEngineAsset* HoudiniEngineAsset;

	/** Viewport client. **/
	TSharedPtr<FHoudiniEngineEditorViewportClient> HoudiniEngineEditorViewportClient;

	/** The scene for this viewport. **/
	FPreviewScene PreviewScene;

};

#endif
