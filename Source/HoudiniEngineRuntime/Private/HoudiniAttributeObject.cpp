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
#include "HoudiniAttributeObject.h"
#include "HoudiniAttributeObjectVersion.h"
#include "HoudiniGeoPartObject.h"


FHoudiniAttributeObject::FHoudiniAttributeObject() :
	ValueCount(0),
	ValueSize(0),
	AttributeName(TEXT("")),
	AttributeOwner(HAPI_ATTROWNER_INVALID),
	StorageType(HAPI_STORAGETYPE_INVALID),
	AssetId(-1),
	ObjectId(-1),
	GeoId(-1),
	PartId(-1),
	HoudiniAttributeObjectFlagsPacked(0u),
	HoudiniAttributeObjectVersion(VER_HOUDINI_ENGINE_ATTRIBUTEOBJECT_BASE)
{

}


FHoudiniAttributeObject::FHoudiniAttributeObject(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
	const char* InAttributeName, const HAPI_AttributeInfo& AttributeInfo) :
	ValueCount(0),
	ValueSize(0),
	AttributeName(FString(InAttributeName)),
	AttributeOwner(AttributeInfo.owner),
	StorageType(AttributeInfo.storage),
	AssetId(HoudiniGeoPartObject.AssetId),
	ObjectId(HoudiniGeoPartObject.ObjectId),
	GeoId(HoudiniGeoPartObject.GeoId),
	PartId(HoudiniGeoPartObject.PartId),
	HoudiniAttributeObjectFlagsPacked(0u),
	HoudiniAttributeObjectVersion(VER_HOUDINI_ENGINE_ATTRIBUTEOBJECT_BASE)
{

}


FHoudiniAttributeObject::FHoudiniAttributeObject(const FHoudiniGeoPartObject& HoudiniGeoPartObject,
	const FString& InAttributeName, const HAPI_AttributeInfo& AttributeInfo) :
	ValueCount(0),
	ValueSize(0),
	AttributeName(InAttributeName),
	AttributeOwner(AttributeInfo.owner),
	StorageType(AttributeInfo.storage),
	AssetId(HoudiniGeoPartObject.AssetId),
	ObjectId(HoudiniGeoPartObject.ObjectId),
	GeoId(HoudiniGeoPartObject.GeoId),
	PartId(HoudiniGeoPartObject.PartId),
	HoudiniAttributeObjectFlagsPacked(0u),
	HoudiniAttributeObjectVersion(VER_HOUDINI_ENGINE_ATTRIBUTEOBJECT_BASE)
{

}


FHoudiniAttributeObject::FHoudiniAttributeObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId, const char* InAttributeName, const HAPI_AttributeInfo& AttributeInfo) :
	ValueCount(0),
	ValueSize(0),
	AttributeName(FString(InAttributeName)),
	AttributeOwner(AttributeInfo.owner),
	StorageType(AttributeInfo.storage),
	AssetId(InAssetId),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	HoudiniAttributeObjectFlagsPacked(0u),
	HoudiniAttributeObjectVersion(VER_HOUDINI_ENGINE_ATTRIBUTEOBJECT_BASE)
{

}


FHoudiniAttributeObject::FHoudiniAttributeObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId, const FString& InAttributeName, const HAPI_AttributeInfo& AttributeInfo) :
	ValueCount(0),
	ValueSize(0),
	AttributeName(InAttributeName),
	AttributeOwner(AttributeInfo.owner),
	StorageType(AttributeInfo.storage),
	AssetId(InAssetId),
	ObjectId(InObjectId),
	GeoId(InGeoId),
	PartId(InPartId),
	HoudiniAttributeObjectFlagsPacked(0u),
	HoudiniAttributeObjectVersion(VER_HOUDINI_ENGINE_ATTRIBUTEOBJECT_BASE)
{

}


FHoudiniAttributeObject::FHoudiniAttributeObject(const FHoudiniAttributeObject& HoudiniAttributeObject) :
	Value(HoudiniAttributeObject.Value),
	ValueCount(HoudiniAttributeObject.ValueCount),
	ValueSize(HoudiniAttributeObject.ValueSize),
	AttributeName(HoudiniAttributeObject.AttributeName),
	AttributeOwner(HoudiniAttributeObject.AttributeOwner),
	StorageType(HoudiniAttributeObject.StorageType),
	AssetId(HoudiniAttributeObject.AssetId),
	ObjectId(HoudiniAttributeObject.ObjectId),
	GeoId(HoudiniAttributeObject.GeoId),
	PartId(HoudiniAttributeObject.PartId),
	HoudiniAttributeObjectFlagsPacked(0u),
	HoudiniAttributeObjectVersion(HoudiniAttributeObject.HoudiniAttributeObjectVersion)
{

}


bool
FHoudiniAttributeObject::HapiGetValues(TArray<int32>& Values, int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!ValueCount || !ValueSize)
	{
		return false;
	}

	Values.SetNumUninitialized(ValueCount * ValueSize);
	FMemory::Memcpy(Values.GetData(), Value.GetData(), ValueCount * ValueSize * sizeof(int32));

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValues(TArray<float>& Values, int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!ValueCount || !ValueSize)
	{
		return false;
	}

	Values.SetNumUninitialized(ValueCount * ValueSize);
	FMemory::Memcpy(Values.GetData(), Value.GetData(), ValueCount * ValueSize * sizeof(float));

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValues(TArray<FHoudiniEngineString>& Values, int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!ValueCount || !ValueSize)
	{
		return false;
	}

	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.Init(-1, ValueCount * ValueSize);
	FMemory::Memcpy(StringHandles.GetData(), Value.GetData(), ValueCount * ValueSize * sizeof(int32));

	for(int32 Idx = 0, Num = StringHandles.Num(); Idx < Num; ++Idx)
	{
		Values.Add(FHoudiniEngineString(StringHandles[Idx]));
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValues(TArray<FString>& Values, int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!ValueCount || !ValueSize)
	{
		return false;
	}

	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.Init(-1, ValueCount * ValueSize);
	FMemory::Memcpy(StringHandles.GetData(), Value.GetData(), ValueCount * ValueSize * sizeof(int32));

	for(int32 Idx = 0, Num = StringHandles.Num(); Idx < Num; ++Idx)
	{
		FString HapiString = TEXT("");
		FHoudiniEngineString HoudiniEngineString(StringHandles[Idx]);
		HoudiniEngineString.ToFString(HapiString);
		Values.Add(HapiString);
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<int32>& Values,
	int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!Vertices.Num())
	{
		return false;
	}

	if(HAPI_ATTROWNER_VERTEX == AttributeOwner)
	{
		// Data is on vertices already, we can do direct transfer.
		return HapiGetValues(Values, TupleSize);
	}
	else
	{
		TArray<int32> UnprocessedValues;
		if(!HapiGetValues(UnprocessedValues, TupleSize))
		{
			return false;
		}

		Values.SetNumUninitialized(Vertices.Num() * TupleSize);

		for(int32 IdxWedge = 0, NumWedges = Vertices.Num(); IdxWedge < NumWedges; ++IdxWedge)
		{
			int32 VertexId = Vertices[IdxWedge];
			int32 PrimIdx = IdxWedge / 3;
			int32 EntryValue = 0;

			for(int32 IdxTuple = 0; IdxTuple < TupleSize; ++IdxTuple)
			{
				switch(AttributeOwner)
				{
					case HAPI_ATTROWNER_POINT:
					{
						EntryValue = UnprocessedValues[VertexId * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_PRIM:
					{
						EntryValue = UnprocessedValues[PrimIdx * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_DETAIL:
					{
						EntryValue = UnprocessedValues[IdxTuple];
						break;
					}

					default:
					{
						Values.Empty();
						return false;
					}
				}

				Values[IdxWedge * TupleSize + IdxTuple] = EntryValue;
			}
		}
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<float>& Values,
	int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!Vertices.Num())
	{
		return false;
	}

	if(HAPI_ATTROWNER_VERTEX == AttributeOwner)
	{
		// Data is on vertices already, we can do direct transfer.
		return HapiGetValues(Values, TupleSize);
	}
	else
	{
		TArray<float> UnprocessedValues;
		if(!HapiGetValues(UnprocessedValues, TupleSize))
		{
			return false;
		}

		Values.SetNumUninitialized(Vertices.Num() * TupleSize);

		for(int32 IdxWedge = 0, NumWedges = Vertices.Num(); IdxWedge < NumWedges; ++IdxWedge)
		{
			int32 VertexId = Vertices[IdxWedge];
			int32 PrimIdx = IdxWedge / 3;
			float EntryValue = 1.0f;

			for(int32 IdxTuple = 0; IdxTuple < TupleSize; ++IdxTuple)
			{
				switch(AttributeOwner)
				{
					case HAPI_ATTROWNER_POINT:
					{
						EntryValue = UnprocessedValues[VertexId * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_PRIM:
					{
						EntryValue = UnprocessedValues[PrimIdx * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_DETAIL:
					{
						EntryValue = UnprocessedValues[IdxTuple];
						break;
					}

					default:
					{
						Values.Empty();
						return false;
					}
				}

				Values[IdxWedge * TupleSize + IdxTuple] = EntryValue;
			}
		}
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<FHoudiniEngineString>& Values,
	int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!Vertices.Num())
	{
		return false;
	}

	if(HAPI_ATTROWNER_VERTEX == AttributeOwner)
	{
		// Data is on vertices already, we can do direct transfer.
		return HapiGetValues(Values, TupleSize);
	}
	else
	{
		TArray<FHoudiniEngineString> UnprocessedValues;
		if(!HapiGetValues(UnprocessedValues, TupleSize))
		{
			return false;
		}

		Values.SetNumUninitialized(Vertices.Num() * TupleSize);

		for(int32 IdxWedge = 0, NumWedges = Vertices.Num(); IdxWedge < NumWedges; ++IdxWedge)
		{
			int32 VertexId = Vertices[IdxWedge];
			int32 PrimIdx = IdxWedge / 3;
			FHoudiniEngineString EntryValue;

			for(int32 IdxTuple = 0; IdxTuple < TupleSize; ++IdxTuple)
			{
				switch(AttributeOwner)
				{
					case HAPI_ATTROWNER_POINT:
					{
						EntryValue = UnprocessedValues[VertexId * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_PRIM:
					{
						EntryValue = UnprocessedValues[PrimIdx * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_DETAIL:
					{
						EntryValue = UnprocessedValues[IdxTuple];
						break;
					}

					default:
					{
						Values.Empty();
						return false;
					}
				}

				Values[IdxWedge * TupleSize + IdxTuple] = EntryValue;
			}
		}
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiGetValuesAsVertex(const TArray<int32>& Vertices, TArray<FString>& Values,
	int32& TupleSize) const
{
	Values.Empty();
	TupleSize = ValueSize;

	if(!Vertices.Num())
	{
		return false;
	}

	if(HAPI_ATTROWNER_VERTEX == AttributeOwner)
	{
		// Data is on vertices already, we can do direct transfer.
		return HapiGetValues(Values, TupleSize);
	}
	else
	{
		TArray<FString> UnprocessedValues;
		if(!HapiGetValues(UnprocessedValues, TupleSize))
		{
			return false;
		}

		Values.SetNumUninitialized(Vertices.Num() * TupleSize);

		for(int32 IdxWedge = 0, NumWedges = Vertices.Num(); IdxWedge < NumWedges; ++IdxWedge)
		{
			int32 VertexId = Vertices[IdxWedge];
			int32 PrimIdx = IdxWedge / 3;
			FString EntryValue = TEXT("");

			for(int32 IdxTuple = 0; IdxTuple < TupleSize; ++IdxTuple)
			{
				switch(AttributeOwner)
				{
					case HAPI_ATTROWNER_POINT:
					{
						EntryValue = UnprocessedValues[VertexId * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_PRIM:
					{
						EntryValue = UnprocessedValues[PrimIdx * TupleSize + IdxTuple];
						break;
					}

					case HAPI_ATTROWNER_DETAIL:
					{
						EntryValue = UnprocessedValues[IdxTuple];
						break;
					}

					default:
					{
						Values.Empty();
						return false;
					}
				}

				Values[IdxWedge * TupleSize + IdxTuple] = EntryValue;
			}
		}
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiIsArray() const
{
	HAPI_AttributeInfo AttributeInfo;
	if(!HapiGetAttributeInfo(AttributeInfo))
	{
		return false;
	}

	return AttributeInfo.count > 1;
}


bool
FHoudiniAttributeObject::HapiRefetch()
{
	Value.Empty();
	ValueCount = 0;
	ValueSize = 0;

	HAPI_AttributeInfo AttributeInfo;
	if(!HapiGetAttributeInfo(AttributeInfo))
	{
		return false;
	}

	ValueCount = AttributeInfo.count;
	ValueSize = AttributeInfo.tupleSize;

	if(!ValueCount || !ValueSize)
	{
		return false;
	}

	bool bResult = true;
	std::string AttributeNameRaw = TCHAR_TO_UTF8(*AttributeName);

	Value.SetNumUninitialized(ValueCount * ValueSize * sizeof(int32));

	HAPI_AttributeInfo ResultAttributeInfo;
	FMemory::Memset<HAPI_AttributeInfo>(ResultAttributeInfo, 0);

	switch(StorageType)
	{
		case HAPI_STORAGETYPE_INT:
		{
			if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), AssetId,
				ObjectId, GeoId, PartId, AttributeNameRaw.c_str(), &ResultAttributeInfo, (int*) &Value[0], 0,
				ValueCount))
			{
				bResult = false;
			}

			break;
		}

		case HAPI_STORAGETYPE_FLOAT:
		{
			if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), AssetId,
				ObjectId, GeoId, PartId, AttributeNameRaw.c_str(), &ResultAttributeInfo, (float*) &Value[0], 0,
				ValueCount))
			{
				bResult = false;
			}

			break;
		}

		case HAPI_STORAGETYPE_STRING:
		{
			if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), AssetId,
				ObjectId, GeoId, PartId, AttributeNameRaw.c_str(), &ResultAttributeInfo, (int*) &Value[0], 0,
				ValueCount))
			{
				bResult = false;
			}

			break;
		}

		default:
		{
			return false;
		}
	}

	if(!bResult)
	{
		Value.Empty();
		ValueCount = 0;
		ValueSize = 0;

		return false;
	}

	return true;
}


bool
FHoudiniAttributeObject::HapiGetAttributeInfo(HAPI_AttributeInfo& AttributeInfo) const
{
	std::string AttributeNameRaw = TCHAR_TO_UTF8(*AttributeName);
	FMemory::Memset<HAPI_AttributeInfo>(AttributeInfo, 0);

	if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), AssetId, ObjectId,
		GeoId, PartId, AttributeNameRaw.c_str(), AttributeOwner, &AttributeInfo))
	{
		return true;
	}

	return false;
}


bool
FHoudiniAttributeObject::HapiExists() const
{
	HAPI_AttributeInfo AttributeInfo;
	if(!HapiGetAttributeInfo(AttributeInfo))
	{
		return false;
	}

	return AttributeInfo.exists;
}


int32
FHoudiniAttributeObject::HapiGetTupleSize() const
{
	HAPI_AttributeInfo AttributeInfo;
	if(!HapiGetAttributeInfo(AttributeInfo))
	{
		return 0;
	}

	return AttributeInfo.tupleSize;
}


bool
FHoudiniAttributeObject::Serialize(FArchive& Ar)
{
	HoudiniAttributeObjectVersion = VER_HOUDINI_ENGINE_ATTRIBUTEOBJECT_AUTOMATIC_VERSION;
	Ar << HoudiniAttributeObjectVersion;

	Ar << HoudiniAttributeObjectFlagsPacked;

	Ar << Value;
	Ar << ValueCount;
	Ar << ValueSize;

	Ar << AttributeName;

	return true;
}


uint32
FHoudiniAttributeObject::GetTypeHash() const
{
	// We hash attribute name.
	return FCrc::Strihash_DEPRECATED(*AttributeName);
}


FArchive&
operator<<(FArchive& Ar, FHoudiniAttributeObject& HoudiniAttributeObject)
{
	HoudiniAttributeObject.Serialize(Ar);
	return Ar;
}


uint32
GetTypeHash(const FHoudiniAttributeObject& HoudiniAttributeObject)
{
	return HoudiniAttributeObject.GetTypeHash();
}
