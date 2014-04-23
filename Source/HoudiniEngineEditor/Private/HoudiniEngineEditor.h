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

class FHoudiniEngineEditor : public IHoudiniEngineEditor, public FGCObject, public FEditorUndoClient, public FNotifyHook
{
public:

	FHoudiniEngineEditor();
	virtual ~FHoudiniEngineEditor();

public: /** IHoudiniEngineEditor interface **/

	virtual UHoudiniEngineAsset* GetHoudiniEngineAsset() const;

public: /** FGCObject interface **/

	virtual void AddReferencedObjects(FReferenceCollector& Collector) OVERRIDE;

public: /** IToolkit interface **/

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;

	virtual FName GetToolkitFName() const OVERRIDE;
	virtual FText GetBaseToolkitName() const OVERRIDE;
	virtual FString GetWorldCentricTabPrefix() const OVERRIDE;

private:

	/** Currently viewed Houdini Engine Asset **/
	UHoudiniEngineAsset* HoudiniEngineAsset;
};
