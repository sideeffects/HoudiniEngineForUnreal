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

#include "CoreMinimal.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniEngineRuntimeCommon.h"

#include "LandscapeProxy.h"
#include "Misc/StringFormatArg.h"
#include "UObject/SoftObjectPtr.h"

#include "HoudiniOutput.generated.h"

class UMaterialInterface;
class ULandscapeLayerInfoObject;



UENUM()
enum class EHoudiniCurveOutputType : uint8
{
	UnrealSpline,
	HoudiniSpline,
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniCurveOutputProperties
{
	GENERATED_USTRUCT_BODY()

	// Curve output properties
	UPROPERTY()
	EHoudiniCurveOutputType CurveOutputType = EHoudiniCurveOutputType::HoudiniSpline;

	UPROPERTY()
	int32 NumPoints = -1;

	UPROPERTY()
	bool bClosed = false;

	UPROPERTY()
	EHoudiniCurveType CurveType = EHoudiniCurveType::Invalid;

	UPROPERTY()
	EHoudiniCurveMethod CurveMethod = EHoudiniCurveMethod::Invalid;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniLandscapePtr : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	FORCEINLINE
	void SetSoftPtr(TSoftObjectPtr<ALandscapeProxy> InSoftPtr) { LandscapeSoftPtr = InSoftPtr; };

	FORCEINLINE
	TSoftObjectPtr<ALandscapeProxy> GetSoftPtr() const { return LandscapeSoftPtr; };

	// Calling Get() during GC will raise an exception because Get calls StaticFindObject.
	FORCEINLINE
	ALandscapeProxy* GetRawPtr() { return !IsGarbageCollecting() ? Cast<ALandscapeProxy>(LandscapeSoftPtr.Get()) : nullptr; };

	FORCEINLINE
	FString GetSoftPtrPath() const { return LandscapeSoftPtr.ToSoftObjectPath().ToString(); };
	
	FORCEINLINE
	void SetLandscapeOutputBakeType(const EHoudiniLandscapeOutputBakeType & InBakeType) { BakeType = InBakeType; };
 
	FORCEINLINE
	EHoudiniLandscapeOutputBakeType GetLandscapeOutputBakeType() const { return BakeType; };
	
	UPROPERTY()
	TSoftObjectPtr<ALandscapeProxy> LandscapeSoftPtr;

	UPROPERTY()
	EHoudiniLandscapeOutputBakeType BakeType;

	// Edit layer to which this output corresponds, if applicable. 
	UPROPERTY()
	FName EditLayerName;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniLandscapeEditLayer : public UObject
{
	GENERATED_BODY()

public:
	FORCEINLINE
	void SetSoftPtr(TSoftObjectPtr<ALandscapeProxy> InSoftPtr) { LandscapeSoftPtr = InSoftPtr; };

	FORCEINLINE
	TSoftObjectPtr<ALandscapeProxy> GetSoftPtr() const { return LandscapeSoftPtr; };

	// Calling Get() during GC will raise an exception because Get calls StaticFindObject.
	FORCEINLINE
	ALandscapeProxy* GetRawPtr() { return !IsGarbageCollecting() ? Cast<ALandscapeProxy>(LandscapeSoftPtr.Get()) : nullptr; };

	FORCEINLINE
	FString GetSoftPtrPath() const { return LandscapeSoftPtr.ToSoftObjectPath().ToString(); };
	
	UPROPERTY()
	TSoftObjectPtr<ALandscapeProxy> LandscapeSoftPtr;

	UPROPERTY()
	FString LayerName;
};


USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniOutputObjectIdentifier
{
	GENERATED_USTRUCT_BODY()

public:
	// Constructors
	FHoudiniOutputObjectIdentifier();
	FHoudiniOutputObjectIdentifier(const int32& InObjectId, const int32& InGeoId, const int32& InPartId, const FString& InSplitIdentifier);

	// Return hash value for this object, used when using this object as a key inside hashing containers.
	uint32 GetTypeHash() const;

	// Comparison operator, used by hashing containers.
	bool operator==(const FHoudiniOutputObjectIdentifier& HoudiniGeoPartObject) const;

	bool Matches(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const;

public:

	// NodeId of corresponding Houdini Object.
	UPROPERTY()
	int32 ObjectId = -1;

	// NodeId of corresponding Houdini Geo.
	UPROPERTY()
	int32 GeoId = -1;

	// PartId
	UPROPERTY()
	int32 PartId = -1;

	// String identifier for the split that created this
	UPROPERTY()
	FString SplitIdentifier = FString();

	// Name of the part used to generate the output
	UPROPERTY()
	FString PartName = FString();

	// First valid primitive index for this output
	// (used to read generic attributes)
	UPROPERTY()
	int32 PrimitiveIndex = -1;

	// First valid point index for this output
	// (used to read generic attributes)
	UPROPERTY()
	int32 PointIndex = -1;

	bool bLoaded = false;
};

/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniOutputObjectIdentifier& HoudiniOutputObjectIdentifier);

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniBakedOutputObjectIdentifier
{
	GENERATED_USTRUCT_BODY()

public:
	// Constructors
	FHoudiniBakedOutputObjectIdentifier();
	FHoudiniBakedOutputObjectIdentifier(const int32& InPartId, const FString& InSplitIdentifier);
	FHoudiniBakedOutputObjectIdentifier(const FHoudiniOutputObjectIdentifier& InIdentifier);

	// Return hash value for this object, used when using this object as a key inside hashing containers.
	uint32 GetTypeHash() const;

	// Comparison operator, used by hashing containers.
	bool operator==(const FHoudiniBakedOutputObjectIdentifier& InIdentifier) const;

public:

	// PartId
	UPROPERTY()
	int32 PartId = -1;

	// String identifier for the split that created this
	UPROPERTY()
	FString SplitIdentifier = FString();
};

/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash(const FHoudiniBakedOutputObjectIdentifier& InIdentifier);

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniInstancedOutput
{
	GENERATED_USTRUCT_BODY()

public:

	void MarkChanged(const bool& InChanged) { bChanged = InChanged; };

	void SetVariationObjectAt(const int32& AtIndex, UObject* InObject);

	bool SetTransformOffsetAt(const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex);

	float GetTransformOffsetAt(const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex);

#if WITH_EDITOR
	void SwitchUniformScaleLock() { bUniformScaleLocked = !bUniformScaleLocked; };
	bool IsUnformScaleLocked() const { return bUniformScaleLocked; };
#endif

public:

	// Original object used by the instancer.
	UPROPERTY()
	TSoftObjectPtr<UObject> OriginalObject = nullptr;

	UPROPERTY()
	int32 OriginalObjectIndex = -1;

	// Original HoudiniGeoPartObject used by the instancer
	//UPROPERTY()
	//FHoudiniGeoPartObject OriginalHGPO;
	
	// Original Instance transforms
	UPROPERTY()
	TArray<FTransform> OriginalTransforms;

	// Variation objects currently used for instancing
	UPROPERTY()
	TArray<TSoftObjectPtr<UObject>> VariationObjects;

	// Transform offsets, one for each variation.
	UPROPERTY()
	TArray<FTransform> VariationTransformOffsets;

	// Index of the variation used for each transform
	UPROPERTY()
	TArray<int32> TransformVariationIndices;

	// Original Indices of the variation instances
	UPROPERTY()
	TArray<int32> OriginalInstanceIndices;

	// Indicates this instanced output's component should be recreated
	UPROPERTY()
	bool bChanged = false;

	// Indicates this instanced output is stale and should be removed
	UPROPERTY()
	bool bStale = false;

	// Indicates if change the scale of Transfrom Offset of this object uniformly
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bUniformScaleLocked = false;
#endif
	// TODO
	// Color overrides??
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniBakedOutputObject
{
	GENERATED_USTRUCT_BODY()

	public:
		FHoudiniBakedOutputObject();

		FHoudiniBakedOutputObject(AActor* InActor, FName InActorBakeName, UObject* InBakeObject=nullptr, UObject* InBakedComponent=nullptr);

		// Returns Actor if valid, otherwise nullptr
		AActor* GetActorIfValid(bool bInTryLoad=true) const;

		// Returns BakedObject if valid, otherwise nullptr
		UObject* GetBakedObjectIfValid(bool bInTryLoad=true) const;

		// Returns BakedComponent if valid, otherwise nullptr
		UObject* GetBakedComponentIfValid(bool bInTryLoad=true) const;

		// Returns Blueprint if valid, otherwise nullptr
		UBlueprint* GetBlueprintIfValid(bool bInTryLoad=true) const;

		// Returns the ULandscapeLayerInfoObject, if valid and found in LandscapeLayers, otherwise nullptr
		ULandscapeLayerInfoObject* GetLandscapeLayerInfoIfValid(const FName& InLayerName, const bool bInTryLoad=true) const;

		// The actor that the baked output was associated with
		UPROPERTY()
		FString Actor;

		// The blueprint that baked output was associated with, if any
		UPROPERTY()
		FString Blueprint;

		// The intended bake actor name. The actor's actual name could have a numeric suffix for uniqueness.
		UPROPERTY()
		FName ActorBakeName = NAME_None;

		// The baked output asset
		UPROPERTY()
		FString BakedObject;

		// The baked output component 
		UPROPERTY()
		FString BakedComponent;

		// In the case of instance actor component baking, this is the array of instanced actors
		UPROPERTY()
		TArray<FString> InstancedActors;

		// In the case of mesh split instancer baking: this is the array of instance components
		UPROPERTY()
		TArray<FString> InstancedComponents;

		// For landscapes this is the previously bake layer info assets (layer name as key, soft object path as value)
		UPROPERTY()
		TMap<FName, FString> LandscapeLayers;
};

// Container to hold the map of baked objects. There should be one of
// these for each UHoudiniOutput. We manage this separately from UHoudiniOutput so
// that the "previous/last" bake objects can survive output reconstruction or PDG
// dirty/dirty all operations.
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniBakedOutput
{
	GENERATED_USTRUCT_BODY()

	public:
		UPROPERTY()
		TMap<FHoudiniBakedOutputObjectIdentifier, FHoudiniBakedOutputObject> BakedOutputObjects;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniOutputObject
{
	GENERATED_USTRUCT_BODY()

	public:

		// The main output object
		UPROPERTY()
		UObject* OutputObject = nullptr;

		// The main output component
		UPROPERTY()
		UObject* OutputComponent = nullptr;

		// Proxy object
		UPROPERTY()
		UObject* ProxyObject = nullptr;

		// Proxy Component
		UPROPERTY()
		UObject* ProxyComponent = nullptr;

		// Mesh output properties
		// If this is true the proxy mesh is "current", 
		// in other words, it is newer than the UStaticMesh
		UPROPERTY()
		bool bProxyIsCurrent = false;

		// Implicit output objects shouldn't be created as actors / components in the scene. 
		UPROPERTY()
		bool bIsImplicit = false;

		// Is this mesh a part of a geometry collection?
		UPROPERTY()
		bool bIsGeometryCollectionPiece = false;

		// Associated geometry collection. Only valid if bIsGeometryCollectionPiece is true;
		// Cached on mesh generation to avoid a Houdini session requirement for baking
		UPROPERTY()
		FString GeometryCollectionPieceName = TEXT("");
	
		// Bake Name override for this output object
		UPROPERTY()
		FString BakeName;

		UPROPERTY()
		FHoudiniCurveOutputProperties CurveOutputProperty;


		// NOTE: The idea behind CachedAttributes and CachedTokens is to
		// collect attributes (such as unreal_level_path and unreal_output_name)
		// and context-specific tokens (hda name, hda actor name, geo and part ids, tile_id, etc)
		// and cache them directly on the output object. When the object gets baked,
		// certain tokens can be updated specifically for the bake pass afterwhich the 
		// the string / path attributes can be resolved with the updated tokens.
		//
		// A more concrete example:
		//  unreal_output_name = "{hda_actor_name}_PurplePlants_{geo_id}_{part_id}"
		//  unreal_level_path  = "{out}/{hda_name}/{guid}/PurplePlants/{geo_id}/{part_id}"
		// 
		// All of the aforementions tokens and attributes would be cached on the output object
		// when it is being cooked so that the same values are available at bake time. During 
		// a bake some tokens may be updated, such as `{out}` to change where assets get serialized.
		
		UPROPERTY()
		TMap<FString,FString> CachedAttributes;

		// Cache any tokens here that is needed for string resolving
		// at bake time. 
		UPROPERTY()
		TMap<FString, FString> CachedTokens;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniOutput : public UObject
{
	GENERATED_UCLASS_BODY()

	// Declare translators as friend so they can easily directly modify
	// and access our HGPO and Output objects
	friend class FHoudiniEditorEquivalenceUtils;
	friend struct FHoudiniMeshTranslator;
	friend struct FHoudiniInstanceTranslator;
	friend struct FHoudiniOutputTranslator;
	friend struct FHoudiniGeometryCollectionTranslator;
	

	virtual ~UHoudiniOutput();

public:

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------
	const EHoudiniOutputType& GetType() const { return Type; };

	const TArray<FHoudiniGeoPartObject>& GetHoudiniGeoPartObjects() const {	return HoudiniGeoPartObjects; };

	// Returns true if we have a HGPO that matches
	const bool HasHoudiniGeoPartObject(const FHoudiniGeoPartObject& InHGPO) const;

	// Returns true if the HGPO is fromn the same HF as us
	const bool HeightfieldMatch(const FHoudiniGeoPartObject& InHGPO, const bool& bVolumeNameShouldMatch) const;

	// Returns the output objects and their corresponding identifiers
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& GetOutputObjects() { return OutputObjects; };

	// Returns the output objects and their corresponding identifiers
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& GetOutputObjects() const { return OutputObjects; };

	// Returns this output's assignement material map
	TMap<FString, UMaterialInterface*>& GetAssignementMaterials() { return AssignementMaterials; };
	
	// Returns this output's replacement material map
	TMap<FString, UMaterialInterface*>& GetReplacementMaterials() { return ReplacementMaterials; };

	// Returns the instanced outputs maps
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput>& GetInstancedOutputs() { return InstancedOutputs; };

	const bool HasGeoChanged() const;
	const bool HasTransformChanged() const;
	const bool HasMaterialsChanged() const;

	// Returns true if there are any proxy objects in output (current or not)
	const bool HasAnyProxy() const;
	// Returns true if the specified identifier has a proxy object (current or not)
	const bool HasProxy(const FHoudiniOutputObjectIdentifier &InIdentifier) const;
	// Returns true if there are any current (most up to date and visible) proxy in the output
	const bool HasAnyCurrentProxy() const;
	// Returns true if the specified identifier's proxy is "current" (in other words, newer than
	// the non-proxy and the proxy should thus be shown instead.
	const bool IsProxyCurrent(const FHoudiniOutputObjectIdentifier &InIdentifier) const;


	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------
	void UpdateOutputType();

	// Adds a new HoudiniGeoPartObject to our array
	void AddNewHGPO(const FHoudiniGeoPartObject& InHGPO);

	// Mark all the current HGPO as stale (from a previous cook)
	// So we can delte them all by calling DeleteAllStaleHGPOs after.
	void MarkAllHGPOsAsStale(const bool& InStale);

	// Delete all the HGPO that were marked as stale
	void DeleteAllStaleHGPOs();

	void SetOutputObjects(const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InOutputObjects) { OutputObjects = InOutputObjects; };

	void SetInstancedOutputs(const TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput>& InInstancedOuput) { InstancedOutputs = InInstancedOuput; };

	// Marks all HGPO and OutputIdentifier as loaded
	void MarkAsLoaded(const bool& InLoaded);

	FORCEINLINE
	const bool IsEditableNode() { return bIsEditableNode; };

	FORCEINLINE
	void SetIsEditableNode(bool IsEditable) { bIsEditableNode = IsEditable; }

	FORCEINLINE
	const bool HasEditableNodeBuilt() { return bHasEditableNodeBuilt; };

	FORCEINLINE
	void SetHasEditableNodeBuilt(bool HasBuilt) { bHasEditableNodeBuilt = HasBuilt; }

	FORCEINLINE
	void SetIsUpdating(bool bInIsUpdating) { bIsUpdating = bInIsUpdating; };

	FORCEINLINE
	bool IsUpdating() const { return bIsUpdating; };

	FORCEINLINE
	void SetLandscapeWorldComposition(const bool bInLandscapeWorldComposition) { bLandscapeWorldComposition = bInLandscapeWorldComposition; };
	
	FORCEINLINE
	bool IsLandscapeWorldComposition () const { return bLandscapeWorldComposition; };

	FORCEINLINE
	TArray<AActor*> & GetHoudiniCreatedSocketActors() { return HoudiniCreatedSocketActors; };

	FORCEINLINE
	TArray<AActor*> & GetHoudiniAttachedSocketActors() { return HoudiniAttachedSocketActors; }

	// Duplicate this object and copy its state to the resulting object.
	// This is typically used to transfer state between between template and instance components.
	UHoudiniOutput* DuplicateAndCopyProperties(UObject* DestOuter, FName NewName);

	// Copy properties but preserve output objects
	virtual void CopyPropertiesFrom(UHoudiniOutput*  InOutput, bool bCopyAllProperties);

	// Set whether this object can delete Houdini nodes.
	void SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes);
	bool CanDeleteHoudiniNodes() const { return bCanDeleteHoudiniNodes; }

	//------------------------------------------------------------------------------------------------
	// Helpers
	//------------------------------------------------------------------------------------------------
	static FString OutputTypeToString(const EHoudiniOutputType& InOutputType);

	FBox GetBounds() const;

	void Clear();

	bool ShouldDeferClear() const;

protected:

	virtual void BeginDestroy() override;

protected:

	// Indicates the type of output we're dealing with
	UPROPERTY()
	EHoudiniOutputType Type;

	// The output's corresponding HGPO
	UPROPERTY()
	TArray<FHoudiniGeoPartObject> HoudiniGeoPartObjects;

	//
	UPROPERTY(DuplicateTransient)
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects;

	// Instanced outputs
	// Stores the instance variations objects (replacement), transform offsets 
	UPROPERTY()
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput> InstancedOutputs;

	// The material assignments for this output
	UPROPERTY()
	TMap<FString, UMaterialInterface*> AssignementMaterials;

	// The material replacements for this output
	UPROPERTY()
	TMap<FString, UMaterialInterface*> ReplacementMaterials;

	// Indicates the number of stale HGPO
	int32 StaleCount;

	UPROPERTY()
	bool bLandscapeWorldComposition;

	// stores the created actors for sockets with actor references.
	// <CreatedActorPtr, SocketName>
	UPROPERTY()
	TArray<AActor*> HoudiniCreatedSocketActors;

	UPROPERTY()
	TArray<AActor*> HoudiniAttachedSocketActors;

private:
	// Use HoudiniOutput to represent an editable curve.
	// This flag tells whether this output is an editable curve.
	UPROPERTY()
	bool bIsEditableNode;

	// An editable node is only built once. This flag indicates whether this node has been built.
	// Transient, so resets every unreal session so curves must be rebuilt to work properly.
	UPROPERTY(Transient, DuplicateTransient)
	bool bHasEditableNodeBuilt;

	// The IsUpdating flag is set to true when this out exists and is being updated.
	UPROPERTY()
	bool bIsUpdating;

	UPROPERTY()
	bool bCanDeleteHoudiniNodes;
};


