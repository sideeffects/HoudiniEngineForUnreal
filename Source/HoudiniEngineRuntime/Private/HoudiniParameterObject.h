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


class FString;
struct HAPI_NodeInfo;
struct HAPI_ParmInfo;
class FHoudiniEngineString;


struct HOUDINIENGINERUNTIME_API FHoudiniParameterObject
{
public:

	FHoudiniParameterObject();
	FHoudiniParameterObject(HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);
	FHoudiniParameterObject(HAPI_NodeId InNodeId, HAPI_ParmId InParmId);
	FHoudiniParameterObject(const FHoudiniParameterObject& HoudiniParameterObject);

public:

	/** Corresponding parameter class. **/
	UClass* GetHoudiniAssetParameterClass() const;

public:

	/** Return corresponding node info structure. **/
	bool HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const;

	/** Return corresponding parameter info. **/
	bool HapiGetParmInfo(HAPI_ParmInfo& ParmInfo) const;

	/** Return name of this parameter. **/
	bool HapiGetName(FString& Name) const;

	/** Return label of this parameter **/
	bool HapiGetLabel(FString& Label) const;

	/** Return help associated with this parameter. **/
	bool HapiGetHelp(FString& Help) const;

	/** Return true if name of this parameter equals to the specified name. **/
	bool HapiIsNameEqual(const FString& Name) const;

	/** Return true if label of this parameter equals to the specified name. **/
	bool HapiIsLabelEqual(const FString& Label) const;

	/** Return child index within immediate parent. **/
	int32 HapiGetChildIndex() const;

	/** Return size. **/
	int32 HapiGetSize() const;

	/** Return multiparm instance index. **/
	int32 HapiGetMultiparmInstanceIndex() const;

	/** Return parent parm id. **/
	HAPI_ParmId HapiGetParentParmId() const;

	/** Get choice count of this parameter. **/
	int32 HapiGetChoiceCount() const;

	/** Return true if this parameter is a child of multiparm. **/
	bool HapiIsChildOfMultiParm() const;

public:

	/** Return type of this parameter. **/
	HAPI_ParmType HapiGetParmType() const;

	/** Return true if this parameter is of a given type. **/
	bool HapiCheckParmType(HAPI_ParmType ParmType) const;

	/** Return true if this parameter falls into integer param category. **/
	bool HapiCheckParmCategoryInteger() const;

	/** Return true if this parameter falls into float param category. **/
	bool HapiCheckParmCategoryFloat() const;

	/** Return true if this parameter falls into string param category. **/
	bool HapiCheckParmCategoryString() const;

	/** Return true if this parameter falls into path param category. **/
	bool HapiCheckParmCategoryPath() const;

	/** Return true if this parameter falls into container param category. **/
	bool HapiCheckParmCategoryContainer() const;

	/** Return true if this parameter falls into non-value param category. **/
	bool HapiCheckParmCategoryNonValue() const;

public:

	/** Return true if this is an array parameter. **/
	bool HapiIsArray() const;

	/** Return true if this is a Substance parameter. **/
	bool HapiIsSubstance() const;

	/** Return true if parameter is visible. **/
	bool HapiIsVisible() const;

	/** Return true if this parameter is enabled. **/
	bool HapiIsEnabled() const;

	/** Return true if this is a spare parameter. **/
	bool HapiIsSpare() const;

	/** Return true if this parameter has min. **/
	bool HapiHasMin() const;

	/** Return true if this parameter has max. **/
	bool HapiHasMax() const;

	/** Return true if this parameter has UI min. **/
	bool HapiHasUiMin() const;

	/** Return true if this parameter has UI max. **/
	bool HapiHasUiMax() const;

	/** Return min value set on this parameter. **/
	float HapiGetMin() const;

	/** Return max value set on this parameter. **/
	float HapiGetMax() const;

	/** Return UI min value set on this parameter. **/
	float HapiGetUiMin() const;

	/** Return UI max value set on this parameter. **/
	float HapiGetUiMax() const;

public:

	/** Return a single value. **/
	bool HapiGetValue(int32& Value) const;
	bool HapiGetValue(float& Value) const;
	bool HapiGetValue(FHoudiniEngineString& Value) const;
	bool HapiGetValue(FString& Value) const;

	/** Return multiple values (for tuples). **/
	bool HapiGetValues(TArray<int32>& Values) const;
	bool HapiGetValues(TArray<float>& Values) const;
	bool HapiGetValues(TArray<FHoudiniEngineString>& Values) const;
	bool HapiGetValues(TArray<FString>& Values) const;

public:

	/** Set a single value. **/
	bool HapiSetValue(int32 Value) const;
	bool HapiSetValue(float Value) const;
	bool HapiSetValue(const FHoudiniEngineString& Value) const;
	bool HapiSetValue(const FString& Value) const;

	/** Set multiple values (for tuples). **/
	bool HapiSetValues(const TArray<int32>& Values) const;
	bool HapiSetValues(const TArray<float>& Values) const;
	bool HapiSetValues(const TArray<FHoudiniEngineString>& Values) const;
	bool HapiSetValues(const TArray<FString>& Values) const;

public:

	/** Return corresponding parm id. **/
	HAPI_ParmId GetParmId() const;

	/** Return corresponding node id. **/
	HAPI_NodeId GetNodeId() const;

public:

	/** Serialization. **/
	void Serialize(FArchive& Ar);

	/** Return hash value for this object, used when using this object as a key inside hashing containers. **/
	uint32 GetTypeHash() const;

	/** Comparison operator, used by hashing containers. **/
	bool operator==(const FHoudiniParameterObject& HoudiniParameterObject) const;

protected:

	/** Parm Id associated with this parameter. **/
	HAPI_ParmId ParmId;

	/** Node Id associated with this parameter. **/
	HAPI_NodeId NodeId;

protected:

	/** Flags used by parameter object. **/
	uint32 HoudiniParameterObjectFlagsPacked;

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniParameterObjectVersion;
};


/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive& operator<<(FArchive& Ar, FHoudiniParameterObject& HoudiniParameterObject);

/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniParameterObject& HoudiniParameterObject);

/** Functor used to sort these objects. **/
struct HOUDINIENGINERUNTIME_API FHoudiniParameterObjectSortPredicate
{
	bool operator()(const FHoudiniParameterObject& A, const FHoudiniParameterObject& B) const;
};
