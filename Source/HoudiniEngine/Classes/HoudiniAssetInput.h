/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Brokers define connection between assets (for example in content
 * browser) and actors.
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
#include "HoudiniAssetInput.generated.h"


UCLASS()
class HOUDINIENGINE_API UHoudiniAssetInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetInput();

public:

	/** Create sintance of this class. **/
	static UHoudiniAssetInput* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, int32 InInputIndex);

public:

	/** Called to destroy connected Houdini asset, if there's one. **/
	void DestroyHoudiniAsset();

public:

	/** Create this parameter from HAPI information - this implementation does nothing as this is not a true parameter. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder);

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue();

public: /** UObject methods. **/

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:

	/** Called when user drops an asset into this input. **/
	void OnAssetDropped(UObject* Object);

	/** Called to determine if asset is acceptable for this input. **/
	bool OnIsAssetAcceptableForDrop(const UObject* Object);

	/** Set value of this property through commit action, used by Slate. **/
	void SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType);

protected:

	/** Return true if this input has connected asset. **/
	bool HasConnectedAsset() const;

protected:

	/** Widget used for dragging and input. **/
	TSharedPtr<SAssetSearchBox> InputWidget;

	/** Object which is used for input. **/
	UObject* InputObject;

	/** Id of the connected asset. **/
	HAPI_AssetId ConnectedAssetId;

	/** Index of this input. **/
	int32 InputIndex;
};
