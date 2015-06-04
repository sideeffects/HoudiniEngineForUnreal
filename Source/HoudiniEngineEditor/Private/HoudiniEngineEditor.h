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
#include "IHoudiniEngineEditor.h"


class IAssetTools;
class IAssetTypeActions;


class FHoudiniEngineEditor : public IHoudiniEngineEditor
{

/** IModuleInterface methods. **/
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

/** IHoudiniEngineEditor methods. **/
public:

	virtual void RegisterComponentVisualizers() override;
	virtual void UnregisterComponentVisualizers() override;
	virtual void RegisterDetails() override;
	virtual void UnregisterDetails() override;
	virtual void RegisterAssetTypeActions() override;
	virtual void UnregisterAssetTypeActions() override;

public:

	/** App identifier string. **/
	static const FName HoudiniEngineEditorAppIdentifier;

public:

	/** Return singleton instance of Houdini Engine Editor, used internally. **/
	static FHoudiniEngineEditor& Get();

	/** Return true if singleton instance has been created. **/
	static bool IsInitialized();

protected:

	/** Register AssetType action. **/
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

private:

	/** Singleton instance of Houdini Engine Editor. **/
	static FHoudiniEngineEditor* HoudiniEngineEditorInstance;

private:

	/** AssetType actions associated with Houdini asset. **/
	TArray<TSharedPtr<IAssetTypeActions> > AssetTypeActions;
	
	/** Visualizer for our spline component. **/
	TSharedPtr<FComponentVisualizer> SplineComponentVisualizer;
};
