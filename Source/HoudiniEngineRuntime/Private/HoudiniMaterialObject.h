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
struct HAPI_MaterialInfo;
struct FHoudiniParameterObject;


struct HOUDINIENGINERUNTIME_API FHoudiniMaterialObject
{
public:

	FHoudiniMaterialObject();
	FHoudiniMaterialObject(const HAPI_MaterialInfo& MaterialInfo);
	FHoudiniMaterialObject(HAPI_AssetId InAssetId, HAPI_NodeId InNodeId);

public:

	/** HAPI: Retrieve corresponding node info structure. **/
	bool HapiGetNodeInfo(HAPI_NodeInfo& NodeInfo) const;

	/** Return parameter objects associated with this material. **/
	bool HapiGetParameterObjects(TArray<FHoudiniParameterObject>& ParameterObjects) const;

	/** Locate parameter object with a specified name **/
	bool HapiLocateParameterByName(const FString& Name, FHoudiniParameterObject& ResultHoudiniParameterObject) const;

	/** Locate parameter object with a specified label. **/
	bool HapiLocateParameterByLabel(const FString& Label, FHoudiniParameterObject& ResultHoudiniParameterObject) const;

public:

	/** Return true if this is a Substance material. **/
	bool IsSubstance() const;

protected:

	/** Asset Id associated with this material. **/
	HAPI_AssetId AssetId;

	/** Node Id associated with this material. **/
	HAPI_NodeId NodeId;
};
