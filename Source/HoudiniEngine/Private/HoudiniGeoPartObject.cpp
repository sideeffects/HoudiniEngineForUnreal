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
	PartName(TEXT("Empty")),
	ObjectId(-1),
	GeoId(-1),
	PartId(-1),
	bIsVisible(true),
	bIsInstancer(false),
	bIsLoaded(false)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FMatrix& InTransform, const FString& InPartName, HAPI_ObjectId InObjectId,
											 HAPI_GeoId InGeoId, HAPI_PartId InPartId, bool bInIsVisible, bool bInIsInstancer) :
	TransformMatrix(InTransform),
	PartName(InPartName),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	bIsVisible(bInIsVisible),
	bIsInstancer(bInIsInstancer),
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
FHoudiniGeoPartObject::operator==(const FHoudiniGeoPartObject& GeoPartObject) const
{
	return (ObjectId == GeoPartObject.ObjectId && GeoId == GeoPartObject.GeoId && PartId == GeoPartObject.PartId && GeoPartObject.bIsLoaded == bIsLoaded);
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
	Ar << PartName;

	Ar << ObjectId;
	Ar << GeoId;
	Ar << PartId;

	Ar << bIsVisible;
	Ar << bIsInstancer;

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
