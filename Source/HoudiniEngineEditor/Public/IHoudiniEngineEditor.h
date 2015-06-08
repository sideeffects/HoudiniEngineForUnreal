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

	/** Register component visualizers used by this module. **/
	virtual void RegisterComponentVisualizers() {}

	/** Unregister components visualizers used by this module. **/
	virtual void UnregisterComponentVisualizers() {}

	/** Register detail presenters used by this module. **/
	virtual void RegisterDetails() {}

	/** Unregister detail presenters used by this module. **/
	virtual void UnregisterDetails() {}

	/** Register asset type actions. **/
	virtual void RegisterAssetTypeActions() {}

	/** Unregister asset type actions. **/
	virtual void UnregisterAssetTypeActions() {}

	/** Create and register asset brokers. **/
	virtual void RegisterAssetBrokers() {}

	/** Destroy and unregister asset brokers. **/
	virtual void UnregisterAssetBrokers() {}

	/** Create and register actor factories. **/
	virtual void RegisterActorFactories() {}
};
