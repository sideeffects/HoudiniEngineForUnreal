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

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"


uint32
GetTypeHash(const FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	return HoudiniGeoPartObject.GetTypeHash();
}


FArchive&
operator<<(FArchive& Ar, FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	HoudiniGeoPartObject.Serialize(Ar);
	return Ar;
}


bool
FHoudiniGeoPartObjectSortPredicate::operator()(const FHoudiniGeoPartObject& A, const FHoudiniGeoPartObject& B) const
{
	if(!A.IsValid() || !B.IsValid())
	{
		return false;
	}

	if(A.ObjectId == B.ObjectId)
	{
		if(A.GeoId == B.GeoId)
		{
			if(A.PartId == B.PartId)
			{
				return A.SplitId < B.SplitId;
			}
			else
			{
				return A.PartId < B.PartId;
			}
		}
		else
		{
			return A.GeoId < B.GeoId;
		}
	}
	
	return A.ObjectId < B.ObjectId;
}


FHoudiniGeoPartObject::FHoudiniGeoPartObject() :
	TransformMatrix(FMatrix::Identity),
	ObjectName(TEXT("Empty")),
	PartName(TEXT("Empty")),
	SplitName(TEXT("")),
	AssetId(-1),
	ObjectId(-1),
	GeoId(-1),
	PartId(-1),
	SplitId(0),
	bIsVisible(true),
	bIsInstancer(false),
	bIsCurve(false),
	bIsEditable(false),
	bHasGeoChanged(false),
	bIsCollidable(false),
	bIsRenderCollidable(false),
	bIsLoaded(false),
	bHasNativeHoudiniMaterial(false),
	bHasUnrealMaterialAssigned(false),
	bNativeHoudiniMaterialRefetch(false),
	bIsTransacting(false)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId) :
	TransformMatrix(FMatrix::Identity),
	ObjectName(TEXT("Empty")),
	PartName(TEXT("Empty")),
	SplitName(TEXT("")),
	AssetId(InAssetId),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	SplitId(0),
	bIsVisible(true),
	bIsInstancer(false),
	bIsCurve(),
	bIsEditable(false),
	bHasGeoChanged(false),
	bIsCollidable(false),
	bIsRenderCollidable(false),
	bIsLoaded(false),
	bHasNativeHoudiniMaterial(false),
	bHasUnrealMaterialAssigned(false),
	bNativeHoudiniMaterialRefetch(false),
	bIsTransacting(false)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FTransform& InTransform, const FString& InObjectName,
	const FString& InPartName, HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId) :
	TransformMatrix(InTransform),
	ObjectName(InObjectName),
	PartName(InPartName),
	SplitName(TEXT("")),
	AssetId(InAssetId),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	SplitId(0),
	bIsVisible(true),
	bIsInstancer(false),
	bIsCurve(false),
	bIsEditable(false),
	bHasGeoChanged(false),
	bIsCollidable(false),
	bIsRenderCollidable(false),
	bIsLoaded(false),
	bHasNativeHoudiniMaterial(false),
	bHasUnrealMaterialAssigned(false),
	bNativeHoudiniMaterialRefetch(false),
	bIsTransacting(false)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FHoudiniGeoPartObject& GeoPartObject, bool bCopyLoaded) :
	TransformMatrix(GeoPartObject.TransformMatrix),
	ObjectName(GeoPartObject.ObjectName),
	PartName(GeoPartObject.PartName),
	SplitName(GeoPartObject.SplitName),
	AssetId(GeoPartObject.AssetId),
	ObjectId(GeoPartObject.ObjectId),
	GeoId(GeoPartObject.GeoId),
	PartId(GeoPartObject.PartId),
	SplitId(GeoPartObject.SplitId),
	bIsVisible(GeoPartObject.bIsVisible),
	bIsInstancer(GeoPartObject.bIsInstancer),
	bIsCurve(GeoPartObject.bIsCurve),
	bIsEditable(GeoPartObject.bIsEditable),
	bHasGeoChanged(GeoPartObject.bHasGeoChanged),
	bIsCollidable(GeoPartObject.bIsCollidable),
	bIsRenderCollidable(GeoPartObject.bIsRenderCollidable),
	bIsLoaded(GeoPartObject.bIsLoaded),
	bHasNativeHoudiniMaterial(GeoPartObject.bHasNativeHoudiniMaterial),
	bHasUnrealMaterialAssigned(GeoPartObject.bHasUnrealMaterialAssigned),
	bNativeHoudiniMaterialRefetch(GeoPartObject.bNativeHoudiniMaterialRefetch),
	bIsTransacting(GeoPartObject.bIsTransacting)
{
	if(bCopyLoaded)
	{
		bIsLoaded = true;
	}
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
FHoudiniGeoPartObject::IsCollidable() const
{
	return bIsCollidable;
}


bool
FHoudiniGeoPartObject::IsRenderCollidable() const
{
	return bIsRenderCollidable;
}


bool
FHoudiniGeoPartObject::operator==(const FHoudiniGeoPartObject& GeoPartObject) const
{
	return (ObjectId == GeoPartObject.ObjectId &&
			GeoId == GeoPartObject.GeoId &&
			PartId == GeoPartObject.PartId &&
			SplitId == GeoPartObject.SplitId);
}


uint32
FHoudiniGeoPartObject::GetTypeHash() const
{
	int32 HashBuffer[4] = { ObjectId, GeoId, PartId, SplitId };
	return FCrc::MemCrc_DEPRECATED((void*) &HashBuffer[0], sizeof(HashBuffer));
}


void
FHoudiniGeoPartObject::Serialize(FArchive& Ar)
{
	Ar << TransformMatrix;

	Ar << ObjectName;
	Ar << PartName;
	Ar << SplitName;

	Ar << AssetId;
	Ar << ObjectId;
	Ar << GeoId;
	Ar << PartId;
	Ar << SplitId;

	Ar << HoudiniGeoPartObjectFlagsPacked;

	if(Ar.IsLoading())
	{
		bIsLoaded = true;
	}

	if(Ar.IsTransacting())
	{
		bIsTransacting = true;
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

	if(IsValid())
	{
		FHoudiniEngineUtils::HapiGetNodeId(InAssetId, ObjectId, GeoId, NodeId);
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
	FHoudiniApi::GetNodeInfo(nullptr, NodeId, &NodeInfo);

	return (NodeInfo.parmCount > 0);
}


void
FHoudiniGeoPartObject::SetUnrealMaterialAssigned()
{
	bHasUnrealMaterialAssigned = true;
}


bool
FHoudiniGeoPartObject::HasUnrealMaterialAssigned() const
{
	return bHasUnrealMaterialAssigned;
}


void
FHoudiniGeoPartObject::ResetUnrealMaterialAssigned()
{
	bHasUnrealMaterialAssigned = false;
	bNativeHoudiniMaterialRefetch = true;
}


bool
FHoudiniGeoPartObject::HasNativeHoudiniMaterial() const
{
	return bHasNativeHoudiniMaterial;
}
