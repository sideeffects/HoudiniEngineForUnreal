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
class FPreviewScene;
class UHoudiniEngineAsset;
class IHoudiniEngineEditor;
class UHoudiniEngineAssetComponent;

class FHoudiniEngineEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FHoudiniEngineEditorViewportClient>
{
public:

	FHoudiniEngineEditorViewportClient(TWeakPtr<IHoudiniEngineEditor> inHoudiniENgineEditor, FPreviewScene& InPreviewScene, UHoudiniEngineAsset* inHoudiniEngineAsset, UHoudiniEngineAssetComponent* inUHoudiniEngineAssetComponent);

private:

	/** Component for Houdini Engine asset. **/
	UHoudiniEngineAssetComponent* HoudiniEngineAssetComponent;

	/** Houdini Engine asset being edited in the editor. **/
	UHoudiniEngineAsset* HoudiniEngineAsset;

	/** Pointer back to editor tool which owns us. **/
	TWeakPtr<IHoudiniEngineEditor> HoudiniEngineEditor;

	/** Flags for various options in the editor. **/

};
#endif
