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
class IComponentAssetBroker;
class UHoudiniAssetComponent;


class FHoudiniEngineEditor : public IHoudiniEngineEditor, public FEditorUndoClient
{
public:
	FHoudiniEngineEditor();

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
	virtual void RegisterAssetBrokers() override;
	virtual void UnregisterAssetBrokers() override;
	virtual void RegisterActorFactories() override;
	virtual void ExtendMenu() override;
	virtual void RegisterStyleSet() override;
	virtual void UnregisterStyleSet() override;
	virtual TSharedPtr<ISlateStyle> GetSlateStyle() const override;
	virtual void RegisterThumbnails() override;
	virtual void UnregisterThumbnails() override;
	virtual void RegisterForUndo() override;
	virtual void UnregisterForUndo() override;

/** FEditorUndoClient methods. **/
public:

	virtual bool MatchesContext(const FString& InContext, UObject* PrimaryObject) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

public:

	/** App identifier string. **/
	static const FName HoudiniEngineEditorAppIdentifier;

public:

	/** Return singleton instance of Houdini Engine Editor, used internally. **/
	static FHoudiniEngineEditor& Get();

	/** Return true if singleton instance has been created. **/
	static bool IsInitialized();

public:

	/** Menu action called to save a HIP file. **/
	void SaveHIPFile();

	/** Helper delegate used to determine if HIP file save can be executed. **/
	bool CanSaveHIPFile() const;

	/** Menu action called to report a bug. **/
	void ReportBug();

	/** Helper delegate used to determine if report a bug can be executed. **/
	bool CanReportBug() const;

protected:

	/** Register AssetType action. **/
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	/** Add menu extension for our module. **/
	void AddHoudiniMenuExtension(FMenuBuilder& MenuBuilder);

private:

	/** Singleton instance of Houdini Engine Editor. **/
	static FHoudiniEngineEditor* HoudiniEngineEditorInstance;

private:

	/** AssetType actions associated with Houdini asset. **/
	TArray<TSharedPtr<IAssetTypeActions> > AssetTypeActions;

	/** Visualizer for our spline component. **/
	TSharedPtr<FComponentVisualizer> HandleComponentVisualizer;

	/** Visualizer for our spline component. **/
	TSharedPtr<FComponentVisualizer> SplineComponentVisualizer;

	/** Broker associated with Houdini asset. **/
	TSharedPtr<IComponentAssetBroker> HoudiniAssetBroker;

	/** The extender to pass to the level editor to extend it's window menu. **/
	TSharedPtr<FExtender> MainMenuExtender;

	/** Slate styleset used by this module. **/
	TSharedPtr<FSlateStyleSet> StyleSet;

	/** Stored last used Houdini component which was involved in undo. **/
	mutable UHoudiniAssetComponent* LastHoudiniAssetComponentUndoObject;
};
