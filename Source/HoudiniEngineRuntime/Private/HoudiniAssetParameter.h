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
#include "HoudiniAssetParameter.generated.h"


class FArchive;
class FReferenceCollector;
class IDetailCategoryBuilder;
class UHoudiniAssetComponent;


UCLASS(config=Editor)
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameter : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameter();

public:

	/** Create this parameter from HAPI information. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

	/** Set component for this parameter. **/
	virtual void SetHoudiniAssetComponent(UHoudiniAssetComponent* InHoudiniAssetComponent);

#if WITH_EDITOR

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& InDetailCategoryBuilder);

	/** Create widget for this parameter inside a given box. **/
	virtual void CreateWidget(TSharedPtr<SVerticalBox> VerticalBox);

#endif

	/** Upload parameter value to HAPI. **/
	virtual bool UploadParameterValue();

	/** Notification from a child parameter about upcoming change. **/
	virtual void NotifyChildParameterWillChange(UHoudiniAssetParameter* HoudiniAssetParameter);

	/** Notifaction from a child parameter about its change. **/
	virtual void NotifyChildParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter);

public:

	/** Return true if this parameter has been changed. **/
	bool HasChanged() const;

	/** Return hash value for this object, used when using this object as a key inside hashing containers. **/
	uint32 GetTypeHash() const;

	/** Return parameter id of this parameter. **/
	HAPI_ParmId GetParmId() const;

	/** Return parameter id of a parent of this parameter. **/
	HAPI_ParmId GetParmParentId() const;

	/** Set parent parameter for this parameter. **/
	void SetParentParameter(UHoudiniAssetParameter* InParentParameter);

	/** Return true if parent parameter exists for this parameter. **/
	bool IsChildParameter() const;

	/** Return parameter name. **/
	const FString& GetParameterName() const;

	/** Return label name. **/
	const FString& GetParameterLabel() const;

	/** Update parameter's node id. This is necessary after parameter is loaded. **/
	void SetNodeId(HAPI_NodeId InNodeId);

	/** Mark this parameter as unchanged. **/
	void UnmarkChanged();

	/** Reset array containing all child parameters. **/
	void ResetChildParameters();

	/** Add a child parameter. **/
	void AddChildParameter(UHoudiniAssetParameter* HoudiniAssetParameter);

	/** Return true if given parameter is an active child parameter. **/
	bool IsActiveChildParameter(UHoudiniAssetParameter* ChildParameter) const;

public:

	/** Helper function to retrieve parameter name from a given param info structure. Returns false if does not exist. **/
	static bool RetrieveParameterName(const HAPI_ParmInfo& ParmInfo, FString& RetrievedName);

	/** Helper function to retrieve label name from a given param info structure. Returns false if does not exist. **/
	static bool RetrieveParameterLabel(const HAPI_ParmInfo& ParmInfo, FString& RetrievedLabel);

private:

	/** Helper function to retrieve HAPI string and convert it to Unreal one. **/
	static bool RetrieveParameterString(HAPI_StringHandle StringHandle, FString& RetrievedName);

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** Set parameter and node ids. **/
	void SetNodeParmIds(HAPI_NodeId InNodeId, HAPI_ParmId InParmId);

	/** Return true if parameter and node ids are valid. **/
	bool HasValidNodeParmIds() const;

	/** Set name and label. If label does not exist, name will be used instead for label. If error occurs, false will be returned. **/
	bool SetNameAndLabel(const HAPI_ParmInfo& ParmInfo);

	/** Set name and label to be same value from string handle. **/
	bool SetNameAndLabel(HAPI_StringHandle StringHandle);

	/** Check if parameter is visible. **/
	bool IsVisible(const HAPI_ParmInfo& ParmInfo) const;

	/** Mark this parameter as pre-changed. This occurs when user modifies the value of this parameter through UI, but before it is saved. **/
	void MarkPreChanged();

	/** Mark this parameter as changed. This occurs when user modifies the value of this parameter through UI. **/
	void MarkChanged();

	/** Record undo information for parameter change. **/
	void RecordUndoState();

	/** Return tuple size. **/
	int32 GetTupleSize() const;

	/** Return true if this parameter is an array (has tuple size larger than one). **/
	bool IsArray() const;

	/** Sets internal value index used by this parameter. **/
	void SetValuesIndex(int32 InValuesIndex);

	/** Return index of active child parameter. **/
	int32 GetActiveChildParameter() const;

#if WITH_EDITOR

	/** Assigns a unique parameter name. **/
	void AssignUniqueParameterName();

#endif

	/** Return true if parameter is spare, that is, created by Houdini Engine only. **/
	bool IsSpare() const;

	/** Return true if parameter is disabled. **/
	bool IsDisabled() const;

protected:

	/** Print parameter information for debugging. **/
	virtual void PrintParameterInfo();

protected:

	/** Array containing all child parameters. **/
	TArray<UHoudiniAssetParameter*> ChildParameters;

#if WITH_EDITOR

	/** Builder used in construction of this parameter. **/
	IDetailCategoryBuilder* DetailCategoryBuilder;

#endif

	/** Owner component. **/
	UHoudiniAssetComponent* HoudiniAssetComponent;

	/** Parent parameter. **/
	UHoudiniAssetParameter* ParentParameter;

	/** Name of this parameter. **/
	FString ParameterName;

	/** Label of this parameter. **/
	FString ParameterLabel;

	/** Node this parameter belongs to. **/
	HAPI_NodeId NodeId;

	/** Id of this parameter. **/
	HAPI_ParmId ParmId;

	/** Id of parent parameter, -1 if root is parent. **/
	HAPI_ParmId ParmParentId;

	/** Tuple size - arrays. **/
	int32 TupleSize;

	/** Internal HAPI cached value index. **/
	int32 ValuesIndex;

	/** Active child parameter. **/
	int32 ActiveChildParameter;

	/** Flags used by this parameter. **/
	union
	{
		struct
		{
			/** Is set to true if this parameter is spare, that is, created by Houdini Engine only. **/
			uint32 bIsSpare : 1;

			/** Is set to true if this parameter is disabled. **/
			uint32 bIsDisabled : 1;

			/** Is set to true if value of this parameter has been changed by user. **/
			uint32 bChanged : 1;

			/** Is set to true when parameter's slider (if it has one) is being dragged. Transient. **/
			uint32 bSliderDragged : 1;
		};

		uint32 HoudiniAssetParameterFlagsPacked;
	};
};


/** Function used by hasing containers to create a unique hash for this type of object. **/
uint32 GetTypeHash(const UHoudiniAssetParameter* HoudiniAssetParameter);
