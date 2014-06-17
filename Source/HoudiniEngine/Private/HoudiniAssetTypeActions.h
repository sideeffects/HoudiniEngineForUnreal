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
class UThumbnailInfo;

class FHoudiniAssetTypeActions : public FAssetTypeActions_Base
{
public: /** FAssetTypeActions_Base methods. **/

	virtual FText GetName() const OVERRIDE;
	virtual FColor GetTypeColor() const OVERRIDE;
	virtual UClass* GetSupportedClass() const OVERRIDE;
	virtual uint32 GetCategories() OVERRIDE;
	virtual UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const OVERRIDE;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const OVERRIDE;
	virtual void GetActions(const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder) OVERRIDE;

protected:

	/** Handler for reload option. **/
	void ExecuteReload(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets);

	/** Handler for reimport option. **/
	void ExecuteReimport(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets);

	/** Handler for when find in explorer is selected */
	void ExecuteFindInExplorer(TArray<TWeakObjectPtr<UHoudiniAsset> > HoudiniAssets);
};
