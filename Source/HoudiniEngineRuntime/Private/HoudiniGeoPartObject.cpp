/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniGeoPartObject.h"

//
FHoudiniGeoPartObject::FHoudiniGeoPartObject()
	: AssetId(-1)
	, AssetName(TEXT(""))
	, ObjectId(-1)
	, ObjectName(TEXT(""))
	, GeoId(-1)
	, PartId(-1)
	, PartName(TEXT(""))
	, bHasCustomPartName(false)
	, TransformMatrix(FMatrix::Identity)
	, NodePath(TEXT(""))
	, Type(EHoudiniPartType::Invalid)
	, InstancerType(EHoudiniInstancerType::Invalid)
	, VolumeName(TEXT(""))
	, bHasEditLayers(false)
	, VolumeTileIndex(-1)
	, bIsVisible(false)
	, bIsEditable(false)
	, bIsTemplated(false)
	, bIsInstanced(false)
	, bHasGeoChanged(true)
	, bHasPartChanged(true)
	, bHasTransformChanged(true)
	, bHasMaterialsChanged(true)
	, bLoaded(false)
{

}

bool
FHoudiniGeoPartObject::IsValid() const
{
	return (ObjectId >= 0 && GeoId >= 0 && PartId >= 0);
}

bool
FHoudiniGeoPartObject::operator==(const FHoudiniGeoPartObject & InGeoPartObject) const
{
	// TODO: split??
	return Equals(InGeoPartObject, true);
}

bool
FHoudiniGeoPartObject::Equals(const FHoudiniGeoPartObject & GeoPartObject, const bool& bIgnoreSplit) const
{
	// TODO: This will likely need some improvement!

	/*
	// Object/Geo/Part IDs must match
	if (ObjectId != GeoPartObject.ObjectId || GeoId != GeoPartObject.GeoId || PartId != GeoPartObject.PartId)
		return false;

	if (!bIgnoreSplit)
	{
		// If the split type and index match, we're equal...
		if (SplitType == GeoPartObject.SplitType && SplitIndex == GeoPartObject.SplitIndex)
			return true;

		// ... if not we should compare our names
		return CompareNames(GeoPartObject, bIgnoreSplit);
	}
	*/

	// See if objects / geo / part ids  match
	bool MatchingIDs = true;
	if (ObjectId != GeoPartObject.ObjectId || GeoId != GeoPartObject.GeoId || PartId != GeoPartObject.PartId)
		MatchingIDs = false;

	// See if the type matches
	bool MatchingType = (Type == GeoPartObject.Type);
	// Both IDs and type match, consider the two HGPO as equals
	if (MatchingIDs && MatchingType)
		return true;

	// Both IDs and type do not match, consider the two HGPOs as different
	if (!MatchingIDs && !MatchingType)
		return false;

	// If only the ID dont match we can do some further checks

	// If one of the two HGPO has been loaded
	if ((bLoaded && !GeoPartObject.bLoaded)
		|| (!bLoaded && GeoPartObject.bLoaded))
	{
		// For loaded HGPOs, part names should be a sufficent comparison
		if (PartName.Equals(GeoPartObject.PartName))
			return true;
	}

	// TODO: This was causing issues somehow with tiled landscapes
	//  ... if not, compare by names
	if(!MatchingIDs)
		return CompareNames(GeoPartObject, bIgnoreSplit);
	
	return false;
}

void 
FHoudiniGeoPartObject::SetCustomPartName(const FString & InName) 
{
	if (InName.IsEmpty())
		return;

	PartName = InName;
	bHasCustomPartName = true;
}

bool
FHoudiniGeoPartObject::CompareNames(const FHoudiniGeoPartObject & HoudiniGeoPartObject, const bool& bIgnoreSplit) const
{
	//TODO: AssetName?

	// Object, part and split names must match
	if (!ObjectName.Equals(HoudiniGeoPartObject.ObjectName)
		|| !PartName.Equals(HoudiniGeoPartObject.PartName))
	{
		return false;
	}

	/*
	// Split should also match if we dont ignore it
	if (!bIgnoreSplit && !SplitName.Equals(HoudiniGeoPartObject.SplitName))
	{
		return false;
	}
	*/

	return true;
}

FString
FHoudiniGeoPartObject::HoudiniPartTypeToString(const EHoudiniPartType& InType)
{
	FString OutTypeStr;
	switch (InType)
	{
		case EHoudiniPartType::Mesh:
			OutTypeStr = TEXT("Mesh");
			break;
		case EHoudiniPartType::Instancer:
			OutTypeStr = TEXT("Instancer");
			break;
		case EHoudiniPartType::Curve:
			OutTypeStr = TEXT("Curve");
			break;
		case EHoudiniPartType::Volume:
			OutTypeStr = TEXT("Volume");
			break;

		default:
		case EHoudiniPartType::Invalid:
			OutTypeStr = TEXT("Invalid");
			break;
	}

	return OutTypeStr;
}