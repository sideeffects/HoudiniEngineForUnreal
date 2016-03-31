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

	/** Return corresponding node info structure. **/
	bool HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const;

	/** Return corresponding parameter info. **/
	bool HapiGetParmInfo(HAPI_ParmInfo& ParmInfo) const;

	/** Return name of this parameter. **/
	bool HapiGetName(FString& Name) const;

	/** Return label of this parameter **/
	bool HapiGetLabel(FString& Label) const;

	/** Return true if name of this parameter equals to the specified name. **/
	bool HapiIsNameEqual(const FString& Name) const;

	/** Return true if label of this parameter equals to the specified name. **/
	bool HapiIsLabelEqual(const FString& Label) const;

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
