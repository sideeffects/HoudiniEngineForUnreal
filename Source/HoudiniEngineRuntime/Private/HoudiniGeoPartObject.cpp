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
#include "HoudiniEngine.h"
#include "HoudiniGeoPartObjectVersion.h"
#include "HoudiniEngineString.h"


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
	InstancerMaterialName(TEXT("")),
	InstancerAttributeMaterialName(TEXT("")),
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
	bPlaceHolderFlags(0),
	bIsTransacting(false),
	bHasCustomName(false),
	bIsBox(false),
	bIsSphere(false),
	bInstancerMaterialAvailable(false),
	bIsVolume(false),
	bInstancerAttributeMaterialAvailable(false),
	HoudiniGeoPartObjectVersion(VER_HOUDINI_ENGINE_GEOPARTOBJECT_BASE)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId) :
	TransformMatrix(FMatrix::Identity),
	ObjectName(TEXT("Empty")),
	PartName(TEXT("Empty")),
	SplitName(TEXT("")),
	InstancerMaterialName(TEXT("")),
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
	bPlaceHolderFlags(0),
	bIsTransacting(false),
	bHasCustomName(false),
	bIsBox(false),
	bIsSphere(false),
	bInstancerMaterialAvailable(false),
	bIsVolume(false),
	bInstancerAttributeMaterialAvailable(false),
	HoudiniGeoPartObjectVersion(VER_HOUDINI_ENGINE_GEOPARTOBJECT_BASE)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FTransform& InTransform, const FString& InObjectName,
	const FString& InPartName, HAPI_AssetId InAssetId, HAPI_ObjectId InObjectId, HAPI_GeoId InGeoId,
	HAPI_PartId InPartId) :
	TransformMatrix(InTransform),
	ObjectName(InObjectName),
	PartName(InPartName),
	SplitName(TEXT("")),
	InstancerMaterialName(TEXT("")),
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
	bPlaceHolderFlags(0),
	bIsTransacting(false),
	bHasCustomName(false),
	bIsBox(false),
	bIsSphere(false),
	bInstancerMaterialAvailable(false),
	bIsVolume(false),
	bInstancerAttributeMaterialAvailable(false),
	HoudiniGeoPartObjectVersion(VER_HOUDINI_ENGINE_GEOPARTOBJECT_BASE)
{

}


FHoudiniGeoPartObject::FHoudiniGeoPartObject(const FHoudiniGeoPartObject& GeoPartObject, bool bCopyLoaded) :
	TransformMatrix(GeoPartObject.TransformMatrix),
	ObjectName(GeoPartObject.ObjectName),
	PartName(GeoPartObject.PartName),
	SplitName(GeoPartObject.SplitName),
	InstancerMaterialName(GeoPartObject.InstancerMaterialName),
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
	bPlaceHolderFlags(GeoPartObject.bPlaceHolderFlags),
	bIsTransacting(GeoPartObject.bIsTransacting),
	bHasCustomName(GeoPartObject.bHasCustomName),
	bIsBox(GeoPartObject.bIsBox),
	bIsSphere(GeoPartObject.bIsSphere),
	bInstancerMaterialAvailable(GeoPartObject.bInstancerMaterialAvailable),
	bIsVolume(GeoPartObject.bIsVolume),
	bInstancerAttributeMaterialAvailable(GeoPartObject.bInstancerAttributeMaterialAvailable),
	HoudiniGeoPartObjectVersion(VER_HOUDINI_ENGINE_GEOPARTOBJECT_BASE)
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
FHoudiniGeoPartObject::IsBox() const
{
	return bIsBox;
}


bool
FHoudiniGeoPartObject::IsSphere() const
{
	return bIsSphere;
}


bool
FHoudiniGeoPartObject::IsVolume() const
{
	return bIsVolume;
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


HAPI_ObjectId
FHoudiniGeoPartObject::GetObjectId() const
{
	return ObjectId;
}


HAPI_GeoId
FHoudiniGeoPartObject::GetGeoId() const
{
	return GeoId;
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
	HoudiniGeoPartObjectVersion = VER_HOUDINI_ENGINE_GEOPARTOBJECT_AUTOMATIC_VERSION;
	Ar << HoudiniGeoPartObjectVersion;

	Ar << TransformMatrix;

	Ar << ObjectName;
	Ar << PartName;
	Ar << SplitName;

	// Serialize instancer material.
	if(HoudiniGeoPartObjectVersion >= VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_MATERIAL_NAME)
	{
		Ar << InstancerMaterialName;
	}

	// Serialize instancer attribute material.
	if(HoudiniGeoPartObjectVersion >= VER_HOUDINI_ENGINE_GEOPARTOBJECT_INSTANCER_ATTRIBUTE_MATERIAL_NAME)
	{
		Ar << InstancerAttributeMaterialName;
	}

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


bool
FHoudiniGeoPartObject::HasParameters() const
{
	return HasParameters(AssetId);
}


bool
FHoudiniGeoPartObject::HasParameters(HAPI_AssetId InAssetId) const
{
	HAPI_NodeId NodeId = HapiGeoGetNodeId(InAssetId);
	if(-1 == NodeId)
	{
		return false;
	}

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo);

	return (NodeInfo.parmCount > 0);
}


bool
FHoudiniGeoPartObject::HasCustomName() const
{
	return bHasCustomName;
}


void
FHoudiniGeoPartObject::SetCustomName(const FString& CustomName)
{
	PartName = CustomName;
	bHasCustomName = true;
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner) const
{
	std::string AttributeNameRaw = "";
	FHoudiniEngineUtils::ConvertUnrealString(AttributeName, AttributeNameRaw);

	return HapiCheckAttributeExistance(AttributeNameRaw, AttributeOwner);
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(const std::string& AttributeName,
	HAPI_AttributeOwner AttributeOwner) const
{
	return FHoudiniEngineUtils::HapiCheckAttributeExists(AssetId, ObjectId, GeoId, PartId, AttributeName.c_str(),
		AttributeOwner);
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(const char* AttributeName, HAPI_AttributeOwner AttributeOwner) const
{
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId) || !IsValid())
	{
		return false;
	}

	return FHoudiniEngineUtils::HapiCheckAttributeExists(AssetId, ObjectId, GeoId, PartId, AttributeName,
		AttributeOwner);
}


HAPI_ObjectId
FHoudiniGeoPartObject::HapiObjectGetToInstanceId() const
{
	HAPI_ObjectId ObjectToInstance = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		ObjectToInstance = ObjectInfo.objectToInstanceId;
	}

	return ObjectToInstance;
}


bool
FHoudiniGeoPartObject::HapiObjectGetInfo(HAPI_ObjectInfo& ObjectInfo) const
{
	return HapiObjectGetInfo(AssetId, ObjectInfo);
}


bool
FHoudiniGeoPartObject::HapiObjectGetInfo(HAPI_AssetId OtherAssetId, HAPI_ObjectInfo& ObjectInfo) const
{
	FMemory::Memset<HAPI_ObjectInfo>(ObjectInfo, 0);

	if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetObjects(FHoudiniEngine::Get().GetSession(),
		OtherAssetId, &ObjectInfo, ObjectId, 1))
	{
		return true;
	}

	return false;
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiObjectGetName() const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		StringHandle = ObjectInfo.nameSH;
	}

	return FHoudiniEngineString(StringHandle);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiObjectGetInstancePath() const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		StringHandle = ObjectInfo.objectInstancePathSH;
	}

	return FHoudiniEngineString(StringHandle);
}


bool
FHoudiniGeoPartObject::HapiObjectIsVisible() const
{
	bool bIsObjectVisible = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		bIsObjectVisible = ObjectInfo.isVisible;
	}

	return bIsObjectVisible;
}


bool
FHoudiniGeoPartObject::HapiObjectIsInstancer() const
{
	bool bIsObjectInstancer = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		bIsObjectInstancer = ObjectInfo.isInstancer;
	}

	return bIsObjectInstancer;
}


bool
FHoudiniGeoPartObject::HapiObjectHasTransformChanged() const
{
	bool bObjectTransformHasChanged = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		bObjectTransformHasChanged = ObjectInfo.hasTransformChanged;
	}

	return bObjectTransformHasChanged;
}


bool
FHoudiniGeoPartObject::HapiObjectHaveGeosChanged() const
{
	bool bGeosChanged = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		bGeosChanged = ObjectInfo.haveGeosChanged;
	}

	return bGeosChanged;
}


int32
FHoudiniGeoPartObject::HapiObjectGetGeoCount() const
{
	int32 GeoCount = 0;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		GeoCount = ObjectInfo.geoCount;
	}

	return GeoCount;
}


HAPI_NodeId
FHoudiniGeoPartObject::HapiObjectGetNodeId() const
{
	int32 GeoCount = 0;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(ObjectInfo))
	{
		GeoCount = ObjectInfo.geoCount;
	}

	return GeoCount;
}


HAPI_NodeId
FHoudiniGeoPartObject::HapiObjectGetNodeId(HAPI_AssetId OtherAssetId) const
{
	HAPI_NodeId NodeId = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		NodeId = ObjectInfo.nodeId;
	}

	return NodeId;
}


bool
FHoudiniGeoPartObject::HapiGeoGetInfo(HAPI_GeoInfo& GeoInfo) const
{
	return HapiGeoGetInfo(AssetId, GeoInfo);
}


bool
FHoudiniGeoPartObject::HapiGeoGetInfo(HAPI_AssetId OtherAssetId, HAPI_GeoInfo& GeoInfo) const
{
	FMemory::Memset<HAPI_GeoInfo>(GeoInfo, 0);

	if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), OtherAssetId, ObjectId,
		GeoId, &GeoInfo))
	{
		return true;
	}

	return false;
}


HAPI_GeoType
FHoudiniGeoPartObject::HapiGeoGetType() const
{
	HAPI_GeoType GeoType = HAPI_GEOTYPE_INVALID;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		GeoType = GeoInfo.type;
	}

	return GeoType;
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiGeoGetName() const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		StringHandle = GeoInfo.nameSH;
	}

	return FHoudiniEngineString(StringHandle);
}


HAPI_NodeId
FHoudiniGeoPartObject::HapiGeoGetNodeId() const
{
	HAPI_NodeId NodeId = -1;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		NodeId = GeoInfo.nodeId;
	}

	return NodeId;
}


HAPI_NodeId
FHoudiniGeoPartObject::HapiGeoGetNodeId(HAPI_AssetId OtherAssetId) const
{
	HAPI_NodeId NodeId = -1;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		NodeId = GeoInfo.nodeId;
	}

	return NodeId;
}


bool
FHoudiniGeoPartObject::HapiGeoIsEditable() const
{
	bool bIsEditable = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		bIsEditable = GeoInfo.isEditable;
	}

	return bIsEditable;
}


bool
FHoudiniGeoPartObject::HapiGeoIsTemplated() const
{
	bool bIsTemplated = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		bIsTemplated = GeoInfo.isTemplated;
	}

	return bIsTemplated;
}


bool
FHoudiniGeoPartObject::HapiGeoIsDisplayGeo() const
{
	bool bIsDisplayGeo = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		bIsDisplayGeo = GeoInfo.isDisplayGeo;
	}

	return bIsDisplayGeo;
}


bool
FHoudiniGeoPartObject::HapiGeoHasChanged() const
{
	bool bGeoChanged = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		bGeoChanged = GeoInfo.hasGeoChanged;
	}

	return bGeoChanged;
}


bool
FHoudiniGeoPartObject::HapiGeoHasMaterialChanged() const
{
	bool bHasMaterialChanged = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		bHasMaterialChanged = GeoInfo.hasMaterialChanged;
	}

	return bHasMaterialChanged;
}


int32
FHoudiniGeoPartObject::HapiGeoGetPointGroupCount() const
{
	int32 PointGroupCount = 0;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		PointGroupCount = GeoInfo.pointGroupCount;
	}

	return PointGroupCount;
}


int32
FHoudiniGeoPartObject::HapiGeoGetPrimitiveGroupCount() const
{
	int32 PrimitiveGroupCount = 0;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		PrimitiveGroupCount = GeoInfo.primitiveGroupCount;
	}

	return PrimitiveGroupCount;
}


int32
FHoudiniGeoPartObject::HapiGeoGetPartCount() const
{
	int32 PartCount = 0;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(GeoInfo))
	{
		PartCount = GeoInfo.partCount;
	}

	return PartCount;
}

