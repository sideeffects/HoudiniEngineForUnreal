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

#include "HoudiniEnginePrivatePCH.h"


uint32
GetTypeHash(const FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	return HoudiniGeoPartObject.GetTypeHash();
}


FHoudiniGeoPartObject::FHoudiniGeoPartObject() :
	TransformMatrix(FMatrix::Identity),
	ObjectName(TEXT("Empty")),
	PartName(TEXT("Empty")),
	AssetId(-1),
	ObjectId(-1),
	GeoId(-1),
	PartId(-1),
	bIsVisible(true),
	bIsInstancer(false),
	bIsCurve(false),
	bIsEditable(false),
	bHasGeoChanged(false),
	bIsLoaded(false)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FMatrix& InTransform, const FString& InObjectName, const FString& InPartName,
											 HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId,
											 bool bInIsVisible, bool bInIsInstancer, bool bInIsCurve, bool bInIsEditable, bool bInHasGeoChanged) :
	TransformMatrix(InTransform),
	ObjectName(InObjectName),
	PartName(InPartName),
	AssetId(InAssetId),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	bIsVisible(bInIsVisible),
	bIsInstancer(bInIsInstancer),
	bIsCurve(bInIsCurve),
	bIsEditable(bInIsEditable),
	bHasGeoChanged(bInHasGeoChanged),
	bIsLoaded(false)
{

}


bool
FHoudiniGeoPartObject::IsVisible() const
{
	return bIsVisible;
}


bool
FHoudiniGeoPartObject::IsInstancer() const
{
	return bIsInstancer;
}


bool
FHoudiniGeoPartObject::IsCurve() const
{
	return bIsCurve;
}


bool
FHoudiniGeoPartObject::IsLoaded() const
{
	return bIsLoaded;
}


bool
FHoudiniGeoPartObject::IsEditable() const
{
	return bIsEditable;
}


bool
FHoudiniGeoPartObject::HasGeoChanged() const
{
	return bHasGeoChanged;
}


bool
FHoudiniGeoPartObject::operator==(const FHoudiniGeoPartObject& GeoPartObject) const
{
	return (ObjectId == GeoPartObject.ObjectId &&
			GeoId == GeoPartObject.GeoId &&
			PartId == GeoPartObject.PartId);
}


uint32
FHoudiniGeoPartObject::GetTypeHash() const
{
	int HashBuffer[3] = { ObjectId, GeoId, PartId };
	return FCrc::MemCrc_DEPRECATED((void*) &HashBuffer[0], sizeof(HashBuffer));
}


void
FHoudiniGeoPartObject::Serialize(FArchive& Ar)
{
	Ar << TransformMatrix;
	Ar << ObjectName;
	Ar << PartName;

	Ar << AssetId;
	Ar << ObjectId;
	Ar << GeoId;
	Ar << PartId;

	Ar << bIsVisible;
	Ar << bIsInstancer;
	Ar << bIsCurve;
	Ar << bIsEditable;

	if(Ar.IsLoading())
	{
		bIsLoaded = true;
	}
}


bool
FHoudiniGeoPartObject::IsValid() const
{
	return (ObjectId >= 0 && GeoId >= 0 && PartId >= 0);
}


bool
FHoudiniGeoPartObject::CompareNames(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const
{
	return (ObjectName == HoudiniGeoPartObject.ObjectName && PartName == HoudiniGeoPartObject.PartName);
}


HAPI_NodeId
FHoudiniGeoPartObject::GetNodeId() const
{
	return GetNodeId(AssetId);
}


HAPI_NodeId
FHoudiniGeoPartObject::GetNodeId(HAPI_AssetId InAssetId) const
{
	HAPI_NodeId NodeId = -1;

	HAPI_GeoInfo GeoInfo;
	if(IsValid() && (-1 != InAssetId) && (HAPI_RESULT_SUCCESS == HAPI_GetGeoInfo(InAssetId, ObjectId, GeoId, &GeoInfo)))
	{
		NodeId = GeoInfo.nodeId;
	}
	else
	{
		FString Str = FHoudiniEngineUtils::GetErrorDescription();
		volatile int i = 42;
	}

	return NodeId;
}


bool
FHoudiniGeoPartObject::HasParameters() const
{
	return HasParameters(AssetId);
}


bool
FHoudiniGeoPartObject::HasParameters(HAPI_AssetId InAssetId) const
{
	HAPI_NodeId NodeId = GetNodeId(InAssetId);
	if(-1 == NodeId)
	{
		return false;
	}

	HAPI_NodeInfo NodeInfo;
	HAPI_GetNodeInfo(NodeId, &NodeInfo);

	return (NodeInfo.parmCount > 0);
}
