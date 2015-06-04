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


class UClass;
class UObject;
class UHoudiniAsset;
class UThumbnailInfo;


class FHoudiniAssetTypeActions : public FAssetTypeActions_Base
{
/** FAssetTypeActions_Base methods. **/
public:

	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder) override;

protected:

	/** Handler for reimport option. **/
	void ExecuteReimport(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets);

	/** Handler for when find in explorer is selected. */
	void ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets);
};
