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
	ObjectId(-1),
	GeoId(-1),
	PartId(-1)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FMatrix& InTransform, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId, HAPI_PartId InPartId) :
	TransformMatrix(InTransform),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId)
{

}


bool
FHoudiniGeoPartObject::operator==(const FHoudiniGeoPartObject& GeoPartObject) const
{
	return (ObjectId == GeoPartObject.ObjectId && GeoId == GeoPartObject.GeoId && PartId == GeoPartObject.PartId);
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
	Ar << ObjectId;
	Ar << GeoId;
	Ar << PartId;
}


bool
FHoudiniGeoPartObject::IsValid() const
{
	return (ObjectId >= 0 && GeoId >= 0 && PartId >= 0);
}
