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


HAPI_PartId
FHoudiniGeoPartObject::GetPartId() const
{
	return PartId;
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
FHoudiniGeoPartObject::HapiCheckAttributeExistance(HAPI_AssetId OtherAssetId, const FString& AttributeName,
	HAPI_AttributeOwner AttributeOwner) const
{
	std::string AttributeNameRaw = "";
	FHoudiniEngineUtils::ConvertUnrealString(AttributeName, AttributeNameRaw);

	return HapiCheckAttributeExistance(OtherAssetId, AttributeNameRaw, AttributeOwner);
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(const FString& AttributeName,
	HAPI_AttributeOwner AttributeOwner) const
{
	return HapiCheckAttributeExistance(AssetId, AttributeName, AttributeOwner);
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
	HAPI_AttributeOwner AttributeOwner) const
{
	return HapiCheckAttributeExistance(AttributeName.c_str(), AttributeOwner);
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(const std::string& AttributeName,
	HAPI_AttributeOwner AttributeOwner) const
{
	return HapiCheckAttributeExistance(AssetId, AttributeName, AttributeOwner);
}

bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(HAPI_AssetId OtherAssetId, const char* AttributeName,
	HAPI_AttributeOwner AttributeOwner) const
{
	HAPI_AttributeInfo AttributeInfo;
	FMemory::Memset<HAPI_AttributeInfo>(AttributeInfo, 0);

	if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), OtherAssetId,
		ObjectId, GeoId, PartId, AttributeName, AttributeOwner, &AttributeInfo))
	{
		return AttributeInfo.exists;
	}

	return false;
}


bool
FHoudiniGeoPartObject::HapiCheckAttributeExistance(const char* AttributeName, HAPI_AttributeOwner AttributeOwner) const
{
	return HapiCheckAttributeExistance(AssetId, AttributeName, AttributeOwner);
}


bool
FHoudiniGeoPartObject::HapiGetInstanceTransforms(HAPI_AssetId OtherAssetId, TArray<FTransform>& AllTransforms)
{
	AllTransforms.Empty();
	int32 PointCount = HapiPartGetPointCount(OtherAssetId);

	if(PointCount > 0)
	{
		TArray<HAPI_Transform> InstanceTransforms;
		InstanceTransforms.SetNumZeroed(PointCount);

		if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetInstanceTransforms(FHoudiniEngine::Get().GetSession(), OtherAssetId,
			ObjectId, GeoId, HAPI_SRT, &InstanceTransforms[0], 0, PointCount))
		{
			for(int32 PointIdx = 0; PointIdx < PointCount; ++PointIdx)
			{
				FTransform TransformMatrix;

				const HAPI_Transform& HapiInstanceTransform = InstanceTransforms[PointIdx];
				FHoudiniEngineUtils::TranslateHapiTransform(HapiInstanceTransform, TransformMatrix);

				AllTransforms.Add(TransformMatrix);
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}


bool
FHoudiniGeoPartObject::HapiGetInstanceTransforms(TArray<FTransform>& AllTransforms)
{
	return HapiGetInstanceTransforms(AssetId, AllTransforms);
}


HAPI_ObjectId
FHoudiniGeoPartObject::HapiObjectGetToInstanceId(HAPI_AssetId OtherAssetId) const
{
	HAPI_ObjectId ObjectToInstance = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		ObjectToInstance = ObjectInfo.objectToInstanceId;
	}

	return ObjectToInstance;
}


HAPI_ObjectId
FHoudiniGeoPartObject::HapiObjectGetToInstanceId() const
{
	return HapiObjectGetToInstanceId(AssetId);
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
FHoudiniGeoPartObject::HapiObjectGetName(HAPI_AssetId OtherAssetId) const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		StringHandle = ObjectInfo.nameSH;
	}

	return FHoudiniEngineString(StringHandle);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiObjectGetName() const
{
	return HapiObjectGetName(AssetId);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiObjectGetInstancePath(HAPI_AssetId OtherAssetId) const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		StringHandle = ObjectInfo.objectInstancePathSH;
	}

	return FHoudiniEngineString(StringHandle);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiObjectGetInstancePath() const
{
	return HapiObjectGetInstancePath(AssetId);
}


bool
FHoudiniGeoPartObject::HapiObjectIsVisible(HAPI_AssetId OtherAssetId) const
{
	bool bIsObjectVisible = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		bIsObjectVisible = ObjectInfo.isVisible;
	}

	return bIsObjectVisible;
}


bool
FHoudiniGeoPartObject::HapiObjectIsVisible() const
{
	return HapiObjectIsVisible(AssetId);
}


bool
FHoudiniGeoPartObject::HapiObjectIsInstancer(HAPI_AssetId OtherAssetId) const
{
	bool bIsObjectInstancer = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		bIsObjectInstancer = ObjectInfo.isInstancer;
	}

	return bIsObjectInstancer;
}


bool
FHoudiniGeoPartObject::HapiObjectIsInstancer() const
{
	return HapiObjectIsInstancer(AssetId);
}


bool
FHoudiniGeoPartObject::HapiObjectHasTransformChanged(HAPI_AssetId OtherAssetId) const
{
	bool bObjectTransformHasChanged = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		bObjectTransformHasChanged = ObjectInfo.hasTransformChanged;
	}

	return bObjectTransformHasChanged;
}


bool
FHoudiniGeoPartObject::HapiObjectHasTransformChanged() const
{
	return HapiObjectHasTransformChanged(AssetId);
}


bool
FHoudiniGeoPartObject::HapiObjectHaveGeosChanged(HAPI_AssetId OtherAssetId) const
{
	bool bGeosChanged = false;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		bGeosChanged = ObjectInfo.haveGeosChanged;
	}

	return bGeosChanged;
}


bool
FHoudiniGeoPartObject::HapiObjectHaveGeosChanged() const
{
	return HapiObjectHaveGeosChanged(AssetId);
}


int32
FHoudiniGeoPartObject::HapiObjectGetGeoCount(HAPI_AssetId OtherAssetId) const
{
	int32 GeoCount = 0;
	HAPI_ObjectInfo ObjectInfo;

	if(HapiObjectGetInfo(OtherAssetId, ObjectInfo))
	{
		GeoCount = ObjectInfo.geoCount;
	}

	return GeoCount;
}


int32
FHoudiniGeoPartObject::HapiObjectGetGeoCount() const
{
	return HapiObjectGetGeoCount(AssetId);
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
FHoudiniGeoPartObject::HapiGeoGetType(HAPI_AssetId OtherAssetId) const
{
	HAPI_GeoType GeoType = HAPI_GEOTYPE_INVALID;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		GeoType = GeoInfo.type;
	}

	return GeoType;
}


HAPI_GeoType
FHoudiniGeoPartObject::HapiGeoGetType() const
{
	return HapiGeoGetType(AssetId);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiGeoGetName(HAPI_AssetId OtherAssetId) const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		StringHandle = GeoInfo.nameSH;
	}

	return FHoudiniEngineString(StringHandle);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiGeoGetName() const
{
	return HapiGeoGetName(AssetId);
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
FHoudiniGeoPartObject::HapiGeoIsEditable(HAPI_AssetId OtherAssetId) const
{
	bool bIsEditable = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		bIsEditable = GeoInfo.isEditable;
	}

	return bIsEditable;
}


bool
FHoudiniGeoPartObject::HapiGeoIsEditable() const
{
	return HapiGeoIsEditable(AssetId);
}


bool
FHoudiniGeoPartObject::HapiGeoIsTemplated(HAPI_AssetId OtherAssetId) const
{
	bool bIsTemplated = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		bIsTemplated = GeoInfo.isTemplated;
	}

	return bIsTemplated;
}


bool
FHoudiniGeoPartObject::HapiGeoIsTemplated() const
{
	return HapiGeoIsTemplated(AssetId);
}


bool
FHoudiniGeoPartObject::HapiGeoIsDisplayGeo(HAPI_AssetId OtherAssetId) const
{
	bool bIsDisplayGeo = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		bIsDisplayGeo = GeoInfo.isDisplayGeo;
	}

	return bIsDisplayGeo;
}


bool
FHoudiniGeoPartObject::HapiGeoIsDisplayGeo() const
{
	return HapiGeoIsDisplayGeo(AssetId);
}


bool
FHoudiniGeoPartObject::HapiGeoHasChanged(HAPI_AssetId OtherAssetId) const
{
	bool bGeoChanged = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		bGeoChanged = GeoInfo.hasGeoChanged;
	}

	return bGeoChanged;
}


bool
FHoudiniGeoPartObject::HapiGeoHasChanged() const
{
	return HapiGeoHasChanged(AssetId);
}


bool
FHoudiniGeoPartObject::HapiGeoHasMaterialChanged(HAPI_AssetId OtherAssetId) const
{
	bool bHasMaterialChanged = false;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		bHasMaterialChanged = GeoInfo.hasMaterialChanged;
	}

	return bHasMaterialChanged;
}


bool
FHoudiniGeoPartObject::HapiGeoHasMaterialChanged() const
{
	return HapiGeoHasMaterialChanged(AssetId);
}


int32
FHoudiniGeoPartObject::HapiGeoGetPointGroupCount(HAPI_AssetId OtherAssetId) const
{
	int32 PointGroupCount = 0;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		PointGroupCount = GeoInfo.pointGroupCount;
	}

	return PointGroupCount;
}


int32
FHoudiniGeoPartObject::HapiGeoGetPointGroupCount() const
{
	return HapiGeoGetPointGroupCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiGeoGetPrimitiveGroupCount(HAPI_AssetId OtherAssetId) const
{
	int32 PrimitiveGroupCount = 0;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		PrimitiveGroupCount = GeoInfo.primitiveGroupCount;
	}

	return PrimitiveGroupCount;
}


int32
FHoudiniGeoPartObject::HapiGeoGetPrimitiveGroupCount() const
{
	return HapiGeoGetPrimitiveGroupCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiGeoGetPartCount(HAPI_AssetId OtherAssetId) const
{
	int32 PartCount = 0;
	HAPI_GeoInfo GeoInfo;

	if(HapiGeoGetInfo(OtherAssetId, GeoInfo))
	{
		PartCount = GeoInfo.partCount;
	}

	return PartCount;
}


int32
FHoudiniGeoPartObject::HapiGeoGetPartCount() const
{
	return HapiGeoGetPartCount(AssetId);
}


bool
FHoudiniGeoPartObject::HapiPartGetInfo(HAPI_PartInfo& PartInfo) const
{
	return HapiPartGetInfo(AssetId, PartInfo);
}


bool
FHoudiniGeoPartObject::HapiPartGetInfo(HAPI_AssetId OtherAssetId, HAPI_PartInfo& PartInfo) const
{
	FMemory::Memset<HAPI_PartInfo>(PartInfo, 0);

	if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), OtherAssetId, ObjectId,
		GeoId, PartId, &PartInfo))
	{
		return true;
	}

	return false;
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiPartGetName(HAPI_AssetId OtherAssetId) const
{
	HAPI_StringHandle StringHandle = -1;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		StringHandle = PartInfo.nameSH;
	}

	return FHoudiniEngineString(StringHandle);
}


FHoudiniEngineString
FHoudiniGeoPartObject::HapiPartGetName() const
{
	return HapiPartGetName(AssetId);
}


HAPI_PartType
FHoudiniGeoPartObject::HapiPartGetType(HAPI_AssetId OtherAssetId) const
{
	HAPI_PartType PartType = HAPI_PARTTYPE_INVALID;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		PartType = PartInfo.type;
	}

	return PartType;
}


HAPI_PartType
FHoudiniGeoPartObject::HapiPartGetType() const
{
	return HapiPartGetType(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetFaceCount(HAPI_AssetId OtherAssetId) const
{
	int32 FaceCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		FaceCount = PartInfo.faceCount;
	}

	return FaceCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetFaceCount() const
{
	return HapiPartGetFaceCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetVertexCount(HAPI_AssetId OtherAssetId) const
{
	int32 VertexCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		VertexCount = PartInfo.vertexCount;
	}

	return VertexCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetVertexCount() const
{
	return HapiPartGetVertexCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetPointCount(HAPI_AssetId OtherAssetId) const
{
	int32 PointCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		PointCount = PartInfo.pointCount;
	}

	return PointCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetPointCount() const
{
	return HapiPartGetPointCount(AssetId);
}


bool
FHoudiniGeoPartObject::HapiPartIsInstanced(HAPI_AssetId OtherAssetId) const
{
	bool bPartIsInstanced = false;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		bPartIsInstanced = PartInfo.isInstanced;
	}

	return bPartIsInstanced;
}


bool
FHoudiniGeoPartObject::HapiPartIsInstanced() const
{
	return HapiPartIsInstanced(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetInstancedPartCount(HAPI_AssetId OtherAssetId) const
{
	int32 InstancedPartCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		InstancedPartCount = PartInfo.instancedPartCount;
	}

	return InstancedPartCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetInstancedPartCount() const
{
	return HapiPartGetInstancedPartCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetInstanceCount(HAPI_AssetId OtherAssetId) const
{
	int32 InstanceCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		InstanceCount = PartInfo.instanceCount;
	}

	return InstanceCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetInstanceCount() const
{
	return HapiPartGetInstanceCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetPointAttributeCount(HAPI_AssetId OtherAssetId) const
{
	int32 PointAttributeCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		PointAttributeCount = PartInfo.pointAttributeCount;
	}

	return PointAttributeCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetPointAttributeCount() const
{
	return HapiPartGetPointAttributeCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetVertexAttributeCount(HAPI_AssetId OtherAssetId) const
{
	int32 VertexAttributeCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		VertexAttributeCount = PartInfo.vertexAttributeCount;
	}

	return VertexAttributeCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetVertexAttributeCount() const
{
	return HapiPartGetVertexAttributeCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetPrimitiveAttributeCount(HAPI_AssetId OtherAssetId) const
{
	int32 PrimitiveAttributeCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		PrimitiveAttributeCount = PartInfo.faceAttributeCount;
	}

	return PrimitiveAttributeCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetPrimitiveAttributeCount() const
{
	return HapiPartGetPrimitiveAttributeCount(AssetId);
}


int32
FHoudiniGeoPartObject::HapiPartGetDetailAttributeCount(HAPI_AssetId OtherAssetId) const
{
	int32 DetailAttributeCount = 0;
	HAPI_PartInfo PartInfo;

	if(HapiPartGetInfo(OtherAssetId, PartInfo))
	{
		DetailAttributeCount = PartInfo.detailAttributeCount;
	}

	return DetailAttributeCount;
}


int32
FHoudiniGeoPartObject::HapiPartGetDetailAttributeCount() const
{
	return HapiPartGetDetailAttributeCount(AssetId);
}


bool
FHoudiniGeoPartObject::HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const char* AttributeName,
	HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo)
{
	FMemory::Memset<HAPI_AttributeInfo>(AttributeInfo, 0);

	if(HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), OtherAssetId, ObjectId,
		GeoId, PartId, AttributeName, AttributeOwner, &AttributeInfo))
	{
		return AttributeInfo.exists;
	}

	return false;
}


bool
FHoudiniGeoPartObject::HapiGetAttributeInfo(const char* AttributeName, HAPI_AttributeOwner AttributeOwner,
	HAPI_AttributeInfo& AttributeInfo)
{
	return HapiGetAttributeInfo(AssetId, AttributeName, AttributeOwner, AttributeInfo);
}


bool
FHoudiniGeoPartObject::HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const std::string& AttributeName,
	HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo)
{
	return HapiGetAttributeInfo(OtherAssetId, AttributeName.c_str(), AttributeOwner, AttributeInfo);
}


bool
FHoudiniGeoPartObject::HapiGetAttributeInfo(const std::string& AttributeName, HAPI_AttributeOwner AttributeOwner,
	HAPI_AttributeInfo& AttributeInfo)
{
	return HapiGetAttributeInfo(AssetId, AttributeName, AttributeOwner, AttributeInfo);
}


bool
FHoudiniGeoPartObject::HapiGetAttributeInfo(HAPI_AssetId OtherAssetId, const FString& AttributeName,
	HAPI_AttributeOwner AttributeOwner, HAPI_AttributeInfo& AttributeInfo)
{
	std::string AttributeNameRaw = "";
	FHoudiniEngineUtils::ConvertUnrealString(AttributeName, AttributeNameRaw);

	return HapiGetAttributeInfo(OtherAssetId, AttributeNameRaw, AttributeOwner, AttributeInfo);
}


bool
FHoudiniGeoPartObject::HapiGetAttributeInfo(const FString& AttributeName, HAPI_AttributeOwner AttributeOwner,
	HAPI_AttributeInfo& AttributeInfo)
{
	return HapiGetAttributeInfo(AssetId, AttributeName, AttributeOwner, AttributeInfo);
}
