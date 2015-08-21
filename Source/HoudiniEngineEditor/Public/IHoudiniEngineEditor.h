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


class IHoudiniEngineEditor : public IModuleInterface
{
public:

	/** Register and unregister component visualizers used by this module. **/
	virtual void RegisterComponentVisualizers() {}
	virtual void UnregisterComponentVisualizers() {}

	/** Register and unregister detail presenters used by this module. **/
	virtual void RegisterDetails() {}
	virtual void UnregisterDetails() {}

	/** Register and unregister asset type actions. **/
	virtual void RegisterAssetTypeActions() {}
	virtual void UnregisterAssetTypeActions() {}

	/** Create and register / unregister asset brokers. **/
	virtual void RegisterAssetBrokers() {}
	virtual void UnregisterAssetBrokers() {}

	/** Create and register actor factories. **/
	virtual void RegisterActorFactories() {}

	/** Extend menu. **/
	virtual void ExtendMenu() {}

	/** Register and unregister Slate style set. **/
	virtual void RegisterStyleSet() {}
	virtual void UnregisterStyleSet() {}

	/** Return Slate style. **/
	virtual TSharedPtr<ISlateStyle> GetSlateStyle() const = 0;

	/** Register and unregister thumbnails. **/
	virtual void RegisterThumbnails() {}
	virtual void UnregisterThumbnails() {}

	/** Register and unregister for undo/redo notifications. **/
	virtual void RegisterForUndo() {}
	virtual void UnregisterForUndo() {}
};
