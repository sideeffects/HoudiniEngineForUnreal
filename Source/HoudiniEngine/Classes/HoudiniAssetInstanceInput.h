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
#include "HoudiniAssetInstanceInput.generated.h"


UCLASS()
class HOUDINIENGINE_API UHoudiniAssetInstanceInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetInstanceInput();

public:

	/** Create sintance of this class. **/
	static UHoudiniAssetInstanceInput* Create(UHoudiniAssetComponent* InHoudiniAssetComponent, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

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
	void OnAssetDropped(UObject* Object, int32 Idx);

	/** Called to determine if asset is acceptable for this input. **/
	bool OnIsAssetAcceptableForDrop(const UObject* Object, int32 Idx);

	/** Set value of this property through commit action, used by Slate. **/
	void SetValueCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 Idx);

public:

	/** Return true if this is an attribute instancer. **/
	bool IsAttributeInstancer() const;

	/** Return true if this is an object instancer. Attribute instancer and object instancers are not mutually exclusive. **/
	bool IsObjectInstancer() const;

protected:

	/** Set object, geo and part information. **/
	void SetObjectGeoPartIds(HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId);

	/** Checks existance of special instance attribute for this instancer. Marks instancer as attribute instancer if it exists. **/
	void CheckInstanceAttribute();

protected:

	/** Widget used for dragging and input. **/
	TArray<TSharedPtr<SAssetSearchBox> > InputWidgets;

	/** Corresponding instanced static mesh components. **/
	TArray<UInstancedStaticMeshComponent*> InstancedStaticMeshComponents;

	/** Static meshes which are used for input. **/
	TArray<UStaticMesh*> StaticMeshes;

	/** Corresponding object id. **/
	HAPI_ObjectId ObjectId;

	/** Id of an object to instance. **/
	HAPI_ObjectId ObjectToInstanceId;

	/** Corresponding geo id. **/
	HAPI_GeoId GeoId;

	/** Corresponding part id. **/
	HAPI_PartId PartId;

	/** Set to true if this is an attribute instancer. **/
	bool bAttributeInstancer;
};
