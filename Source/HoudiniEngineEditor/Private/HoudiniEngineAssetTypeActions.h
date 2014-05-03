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

class IToolkitHost;
class FMenuBuilder;

class FHoudiniEngineAssetTypeActions : public FAssetTypeActions_Base
{
public: /** FAssetTypeActions_Base **/

	virtual FText GetName() const OVERRIDE;
	virtual FColor GetTypeColor() const OVERRIDE;
	virtual UClass* GetSupportedClass() const OVERRIDE;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) OVERRIDE;
	virtual uint32 GetCategories() OVERRIDE;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const OVERRIDE;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) OVERRIDE;

private:

	/** Handler for when Edit is selected. **/
	void ExecuteEdit(TArray<TWeakObjectPtr<UHoudiniEngineAsset> > Objects);

	/** Handler for when FindInExplorer is selected. **/
	void ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniEngineAsset> > Objects);
};
