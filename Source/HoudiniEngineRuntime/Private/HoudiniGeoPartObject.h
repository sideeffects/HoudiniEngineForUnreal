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

#pragma once

#include "HoudiniEngineRuntimeCommon.h"

#include "HoudiniGeoPartObject.generated.h"

UENUM()
enum class EHoudiniGeoType : uint8
{
	Invalid,

	Default,
	Intermediate,
	Input,
	Curve
};

UENUM()
enum class EHoudiniPartType : uint8
{
	Invalid,

	Mesh,
	Instancer,
	Curve,
	Volume
};

UENUM()
enum class EHoudiniInstancerType : uint8
{
	Invalid,

	ObjectInstancer,
	PackedPrimitive,
	AttributeInstancer,
	OldSchoolAttributeInstancer,
	GeometryCollection
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniObjectInfo
{
	GENERATED_USTRUCT_BODY()

	FString Name = TEXT("");

	int32 NodeId = -1;
	int32 ObjectToInstanceID = -1;

	bool bHasTransformChanged = false;
	bool bHaveGeosChanged = false;
	bool bIsVisible = false;
	bool bIsInstancer = false;
	bool bIsInstanced = false;

	int32 GeoCount = -1;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniGeoInfo
{
	GENERATED_USTRUCT_BODY()

	EHoudiniGeoType Type = EHoudiniGeoType::Invalid;
	FString Name = TEXT("");
	int32 NodeId = -1;

	bool bIsEditable = false;
	bool bIsTemplated = false;
	bool bIsDisplayGeo = false;
	bool bHasGeoChanged = false;
	bool bHasMaterialChanged = false;

	int32 PartCount = -1;
	int32 PointGroupCount = -1;
	int32 PrimitiveGroupCount = -1;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniPartInfo
{
	GENERATED_USTRUCT_BODY()

	int32 PartId = -1;
	FString Name = TEXT("");

	EHoudiniPartType Type = EHoudiniPartType::Invalid;

	int32 FaceCount = -1;
	int32 VertexCount = -1;
	int32 PointCount = -1;

	int32 PointAttributeCounts = -1;
	int32 VertexAttributeCounts = -1;
	int32 PrimitiveAttributeCounts = -1;
	int32 DetailAttributeCounts = -1;

	bool bIsInstanced = false;

	int32 InstancedPartCount = -1;
	int32 InstanceCount = -1;

	bool bHasChanged = false;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniVolumeInfo
{
	GENERATED_USTRUCT_BODY()

	FString Name = TEXT("");
	bool bIsVDB = false; // replaces VolumeType Type;

	int32 TupleSize = -1;
	bool bIsFloat = false; // replaces StorageType StorageType;
	int32 TileSize = -1;

	FTransform Transform = FTransform::Identity;
	bool bHasTaper = false;

	int32 XLength = -1;
	int32 YLength = -1;
	int32 ZLength = -1;

	int32 MinX = -1;
	int32 MinY = -1;
	int32 MinZ = -1;

	float XTaper = 0.0f;
	float YTaper = 0.0f;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniCurveInfo
{
	GENERATED_USTRUCT_BODY()

	EHoudiniCurveType Type = EHoudiniCurveType::Invalid;

	int32 CurveCount = -1;
	int32 VertexCount = -1;
	int32 KnotCount = -1;

	bool bIsPeriodic = false;
	bool bIsRational = false;

	int32 Order = -1;

	bool bHasKnots = false;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniMeshSocket
{
	GENERATED_USTRUCT_BODY()

	// Equality operator, used by containers
	bool operator==(const FHoudiniMeshSocket& InSocket) const
	{
		return Transform.Equals(InSocket.Transform)
			&& Name == InSocket.Name
			&& Actor == InSocket.Actor
			&& Tag == InSocket.Tag;
	}

	// Members
	FTransform Transform = FTransform::Identity;
	FString Name = TEXT("Socket");
	FString Actor = FString();
	FString Tag = FString();
};

/*
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniSplitDataCache
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString SplitName;
	//UPROPERTY()
	//FHoudiniOutputObjectIdentifier SplitIdentifier;
	//EHoudiniSplitType SplitType;

	UPROPERTY()
	TArray<FVector> Positions;
	UPROPERTY()
	TArray<uint32> Indices;

	UPROPERTY()
	TArray<FVector> Normals;
	UPROPERTY()
	TArray<FVector> Tangents;
	UPROPERTY()
	TArray<FVector> Binormals;
	
	UPROPERTY()
	TArray<FVector4> Colors;

	//UPROPERTY()
	//TArray<TArray<FVector2D>> UVs;
	
	//TArray<bool> EdgeHardnesses;	
	UPROPERTY()
	TArray<int32> FaceSmoothingMasks;
	UPROPERTY()
	TArray<int32> LightMapResolutions;

	UPROPERTY()
	TArray<int32> MaterialIndices;
	UPROPERTY()
	TArray<UMaterialInterface*> Materials;

	UPROPERTY()
	float lod_screensize;

	UPROPERTY()
	FKAggregateGeom AggregateCollisions;
};
*/


USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniGeoPartObject
{
public:

	GENERATED_USTRUCT_BODY()

	FHoudiniGeoPartObject();

	// Indicates if this HGPO is valid
	bool IsValid() const;

	// Equality operator, used by containers
	bool operator==(const FHoudiniGeoPartObject & GeoPartObject) const;	

	// Checks equality, with the possibility to ignore the HGPO's splits
	bool Equals(const FHoudiniGeoPartObject & GeoPartObject, const bool& bIgnoreSplit) const;

	// Comparison based on object/part/split name.
	bool CompareNames(const FHoudiniGeoPartObject & HoudiniGeoPartObject, const bool& bIgnoreSplit) const;

	void SetCustomPartName(const FString & InName);

	static FString HoudiniPartTypeToString(const EHoudiniPartType& InType);

public:

	// NodeId of corresponding HAPI Asset.
	UPROPERTY()
	int32 AssetId;

	// Name of corresponding HDA.
	UPROPERTY()
	FString AssetName;

	// NodeId of corresponding HAPI Object.
	UPROPERTY()
	int32 ObjectId;

	// Name of associated object.
	UPROPERTY()
	FString ObjectName;

	// NodeId of corresponding HAPI Geo.
	UPROPERTY()
	int32 GeoId;

	// PartId of corresponding HAPI Part.
	UPROPERTY()
	int32 PartId;

	// Name of associated part.
	UPROPERTY()
	FString PartName;

	UPROPERTY()
	bool bHasCustomPartName;

	/*
	// Type of the split.
	UPROPERTY()
	EHoudiniSplitType SplitType;

	// Index of a split. In most cases this will be 0.
	UPROPERTY()
	int32 SplitIndex;

	// Name of group which was used for splitting, empty if there's none.
	UPROPERTY()
	FString SplitName;
	*/

	// Split groups handled by this HGPO
	UPROPERTY()
	TArray<FString> SplitGroups;

	// Transform of this geo part object.
	UPROPERTY()
	FTransform TransformMatrix;	

	// Path to the corresponding node
	UPROPERTY()
	FString NodePath;

	// Indicates the type of the referenced object
	UPROPERTY()
	EHoudiniPartType Type;

	// Indicates the type of instancer
	UPROPERTY()
	EHoudiniInstancerType InstancerType;

	// 
	UPROPERTY()
	FString VolumeName;

	UPROPERTY()
	bool bHasEditLayers = false;

	// Name of edit layer 
	UPROPERTY()
	FString VolumeLayerName;

	//
	UPROPERTY()
	int32 VolumeTileIndex;

	// Is set to true when referenced object is visible.
	UPROPERTY()
	bool bIsVisible;
	
	// Is set to true when referenced object is editable.
	UPROPERTY()
	bool bIsEditable;

	// Is set to true when referenced object is templated.
	UPROPERTY()
	bool bIsTemplated;

	// Is set to true when the referenced object is instanced.
	UPROPERTY()
	bool bIsInstanced;

	// Indicates the parent geo has changed and needs to be rebuilt
	UPROPERTY()
	bool bHasGeoChanged;

	// Indicates the part has changed and needs to be rebuilt
	UPROPERTY()
	bool bHasPartChanged;

	// Indicates only the transform needs to be updated
	UPROPERTY()
	bool bHasTransformChanged;

	// Indicates only the material needs to be updated
	UPROPERTY()
	bool bHasMaterialsChanged;

	// Indicates this object has been loaded
	bool bLoaded;

	// We also keep a cache of the various info objects
	// That we've extracted from HAPI
	
	// ObjectInfo cache
	FHoudiniObjectInfo ObjectInfo;
	// GeoInfo cache
	FHoudiniGeoInfo GeoInfo;
	// PartInfo cache
	FHoudiniPartInfo PartInfo;
	// VolumeInfo cache
	FHoudiniVolumeInfo VolumeInfo;
	// CurveInfo cache
	FHoudiniCurveInfo CurveInfo;

	// Stores the Mesh Sockets found for a given HGPO
	UPROPERTY()
	TArray<FHoudiniMeshSocket> AllMeshSockets;

	// Cache of this HGPO split data
	//TArray<FHoudiniSplitDataCache> SplitCache;
};