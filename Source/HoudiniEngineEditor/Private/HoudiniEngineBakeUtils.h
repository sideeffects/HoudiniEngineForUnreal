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

#include "HoudiniPDGAssetLink.h"
#include "HoudiniOutput.h"
#include "HoudiniPackageParams.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Materials/MaterialExpression.h"
#include "HoudiniOutputDetails.h"
#include "HoudiniEngineOutputStats.h"

struct FHoudiniEngineBakedActor;
class UDataTable;
class ULevelInstanceComponent;
class UHoudiniAssetComponent;
class UHoudiniOutput;
class ALandscapeProxy;
class UStaticMesh;
class USkeletalMesh;
class USplineComponent;
class UPackage;
class UWorld;
class AActor;
class UHoudiniSplineComponent;
class UStaticMeshComponent;
class UHoudiniPDGAssetLink;
class UTOPNetwork;
class UTOPNode;
class UGeometryCollection;
class UGeometryCollectionComponent;
class AGeometryCollectionActor; 
class UMaterialExpression;
class UPhysicsAsset;

struct FHoudiniPackageParams;
struct FHoudiniGeoPartObject;
struct FHoudiniOutputObject;
struct FHoudiniOutputObjectIdentifier;
struct FHoudiniEngineOutputStats;
struct FHoudiniBakedOutputObject;
struct FHoudiniAttributeResolver;

enum class EHoudiniLandscapeOutputBakeType : uint8;

// Use this structure to pass bake settings to bake functions. Previously the code would
// pass each of there individually which was hard to maintain.
struct FHoudiniBakeSettings
{
	void SetFromHAC(UHoudiniAssetComponent * HAC);

	bool bReplaceAssets = false;
	bool bReplaceActors = false;
	bool bRecenterBakedActors = false;
	EHoudiniEngineActorBakeOption ActorBakeOption = EHoudiniEngineActorBakeOption::OneActorPerComponent;
	FString DefaultBakeName = TEXT("{hda_actor_name}_{guid8}");
};

// Use this structure to return bake output data. Previously the code would
// pass each of there individually which was hard to maintain.
struct FHoudiniBakedObjectData
{
	TArray<UBlueprint*> Blueprints;
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
};


// An enum of the different types for instancer component/bake types
UENUM()
enum class EHoudiniInstancerComponentType : uint8
{
	// Single static mesh component
	StaticMeshComponent,
	// (Hierarichal)InstancedStaticMeshComponent
	InstancedStaticMeshComponent,
	MeshSplitInstancerComponent,
	InstancedActorComponent,
	// For baking foliage as foliage
	FoliageInstancedStaticMeshComponent,
	// Baking foliage as HISMC
	FoliageAsHierarchicalInstancedStaticMeshComponent,
	GeometryCollectionComponent
};

// Helper struct to track actors created/used when baking, with
// the intended bake name (before making it unique), and their
// output index and output object identifier.
struct HOUDINIENGINEEDITOR_API FHoudiniEngineBakedActor
{
	FHoudiniEngineBakedActor();

	FHoudiniEngineBakedActor(
		AActor* InActor,
		FName InActorBakeName,
		FName InWorldOutlinerFolder,
		int32 InOutputIndex,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		UObject* InBakedObject,
		UObject* InSourceObject,
		UObject* InBakedComponent,
		const FString& InBakeFolderPath,
		const FHoudiniPackageParams& InBakedObjectPackageParams);

	// The actor that the baked output was associated with
	AActor* Actor = nullptr;

	// The output index on the HAC for the baked object
	int32 OutputIndex = INDEX_NONE;

	// The output object identifier for the baked object
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier;

	// The intended bake actor name. The actor's actual name could have a numeric suffix for uniqueness.
	FName ActorBakeName = NAME_None;

	// The world outliner folder the actor is placed in
	FName WorldOutlinerFolder = NAME_None;

	// The array index of the work result when baking PDG
	int32 PDGWorkResultArrayIndex = INDEX_NONE;

	// The work item index (as returned by HAPI) for the work item/work result, used when baking PDG
	int32 PDGWorkItemIndex = INDEX_NONE;

	// The array index of the work result object of the work result when baking PDG
	int32 PDGWorkResultObjectArrayIndex = INDEX_NONE;

	// The baked primary asset (such as static mesh)
	UObject* BakedObject = nullptr;

	// The temp asset that was baked to BakedObject
	UObject* SourceObject = nullptr;

	// The baked component or foliage type in the case of foliage
	UObject* BakedComponent = nullptr;

	// The bake folder path to where BakedObject was baked
	FString BakeFolderPath;

	// The package params for the BakedObject
	FHoudiniPackageParams BakedObjectPackageParams;

	// True if this entry was created by an instancer output.
	bool bInstancerOutput;
	
	// The package params built for the instancer part of the output, if this was an instancer.
	// This would mostly be useful in situations for we later need the resolver and/or cached attributes and
	// tokens, such as for blueprint baking.
	FHoudiniPackageParams InstancerPackageParams;

	// Used to delay all post bake calls so they are done only once per baked actor
	bool bPostBakeProcessPostponed = false;
};

// Helper class to keep track of various parts of baking state while baking outputs from a HAC.
class FHoudiniEngineBakeState
{
public:

	FHoudiniEngineBakeState() = delete;

	// Initialize the bake state by the number of outputs and previous baked outputs.
	FHoudiniEngineBakeState(const int32 InNumOutputs, const TArray<FHoudiniBakedOutput>& InOldBakedOutputs);

	// Find an entry from OldBakedOutputs by index an identifier.
	const FHoudiniBakedOutputObject* FindOldBakedOutputObject(
		const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier);

	// Make a new baked output object that is populated from the old entry with index and identifier. Does not add the
	// new entry to NewBakedOutputs, use SetNewBakedOutputObject() for that.
	FHoudiniBakedOutputObject MakeNewBakedOutputObject(
		const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier, bool& bOutHasPreviousBakeData);

	// Find an entry in NewBakedOutputs by index and identifier.
	const FHoudiniBakedOutputObject& FindNewBakedOutputObjectChecked(
		const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier);

	// Find an entry in NewBakedOutputs by index and identifier.
	FHoudiniBakedOutputObject& FindOrAddNewBakedOutputObject(
		const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier);

	// Set an entry in NewBakedOutputs by index and identifier.
	FHoudiniBakedOutputObject& SetNewBakedOutputObject(
		const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier, const FHoudiniBakedOutputObject& InBakedOutputObject);

	// Set an entry in NewBakedOutputs by index and identifier.
	FHoudiniBakedOutputObject& SetNewBakedOutputObject(
		const int32 InOutputIndex, const FHoudiniOutputObjectIdentifier& InIdentifier, FHoudiniBakedOutputObject&& InBakedOutputObject);

	// Get a const reference to OldBakedOutputs
	const TArray<FHoudiniBakedOutput>& GetOldBakedOutputs() const { return OldBakedOutputs; }
	// Get a reference to OldBakedOutputs
	TArray<FHoudiniBakedOutput>& GetOldBakedOutputs() { return OldBakedOutputs; }

	// Get a const reference to NewBakedOutputs
	const TArray<FHoudiniBakedOutput>& GetNewBakedOutputs() const { return NewBakedOutputs; }
	// Get a reference to NewBakedOutputs
	TArray<FHoudiniBakedOutput>& GetNewBakedOutputs() { return NewBakedOutputs; }

	// Get a const reference to BakedSkeletons
	const TMap<USkeleton*, USkeleton*>& GetBakedSkeletons() const { return BakedSkeletons; }
	// Get a reference to BakedSkeletons
	TMap<USkeleton*, USkeleton*>& GetBakedSkeletons() { return BakedSkeletons; }

	// Get a const reference to BakedSkeletons
	const TMap<UPhysicsAsset*, UPhysicsAsset*>& GetBakedPhysicsAssets() const { return BakedPhysicsAssets; }
	// Get a reference to BakedSkeletons
	TMap<UPhysicsAsset*, UPhysicsAsset*>& GetBakedPhysicsAssets() { return BakedPhysicsAssets; }

	// Add a temp -> baked skeleton entry
	void AddBakedSkeleton(USkeleton* InTempSkeleton, USkeleton* InBakedSkeleton) { BakedSkeletons.Add(InTempSkeleton, InBakedSkeleton); }
	// Look for a baked skeleton for InTempSkeleton in BakedSkeletons map.
	USkeleton* FindBakedSkeleton(USkeleton const* InTempSkeleton, bool& bFoundEntry) const;

protected:
	// Array of old / previous baked outputs
	TArray<FHoudiniBakedOutput> OldBakedOutputs;
	// Array of new  baked outputs
	TArray<FHoudiniBakedOutput> NewBakedOutputs;

	// Map of temp to baked Skeletons
	TMap<USkeleton*, USkeleton*> BakedSkeletons;

	// Map of temp to baked Physics Assets
	TMap<UPhysicsAsset*, UPhysicsAsset*> BakedPhysicsAssets;
};


struct HOUDINIENGINEEDITOR_API FHoudiniEngineBakeUtils
{
public:

	/** Bake static mesh. **/

	/*static UStaticMesh * BakeStaticMesh(
		UHoudiniAssetComponent * HoudiniAssetComponent,
		UStaticMesh * InStaticMesh,
		const FHoudiniPackageParams &PackageParams);*/

	static ALandscapeProxy* BakeHeightfield(
		ALandscapeProxy * InLandscapeProxy,
		const FHoudiniPackageParams &PackageParams,
		const EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType,
		FHoudiniBakedObjectData& BakedObjectData);

	static bool BakeCurve(
		UHoudiniAssetComponent const* const InHoudiniAssetComponent,
		USplineComponent* InSplineComponent,
		ULevel* InLevel,
		const FHoudiniPackageParams &PackageParams,
		const FHoudiniBakeSettings& BakeSettings,
		const FName& InActorName,
		AActor*& OutActor,
		USplineComponent*& OutSplineComponent,
		FHoudiniBakedObjectData& BakedObjectData,
		FName InOverrideFolderPath=NAME_None,
		AActor* InActor=nullptr,
		TSubclassOf<AActor> BakeActorClass = nullptr);

	static bool BakeCurve(
		UHoudiniAssetComponent const* const InHoudiniAssetComponent,
		const FHoudiniOutputObject& InOutputObject,
		FHoudiniBakedOutputObject& InBakedOutputObject,
		// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
		const FHoudiniPackageParams &PackageParams,
		FHoudiniAttributeResolver& InResolver,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		FHoudiniEngineBakedActor& OutBakedActorEntry,
		FHoudiniBakedObjectData& BakedObjectData,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder="");

	static AActor* BakeInputHoudiniCurveToActor(
		UHoudiniAssetComponent const* const InHoudiniAssetComponent,
		UHoudiniSplineComponent * InHoudiniSplineComponent,
		const FHoudiniPackageParams & PackageParams,
		const FHoudiniBakeSettings& BakeSettings,
		UWorld* WorldToSpawn,
		const FTransform & SpawnTransform);

	static UBlueprint* BakeInputHoudiniCurveToBlueprint(
		UHoudiniAssetComponent const* const InHoudiniAssetComponent,
		UHoudiniSplineComponent * InHoudiniSplineComponent,
		const FHoudiniPackageParams & PackageParams,
		const FHoudiniBakeSettings& BakeSettings,
		UWorld* WorldToSpawn,
		const FTransform & SpawnTransform);

	static UStaticMesh* BakeStaticMesh(
		UStaticMesh * StaticMesh,
		const FHoudiniPackageParams & PackageParams,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		const FDirectoryPath& InTempCookFolder,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap);

	static bool BakeInstancerOutputToActors(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FTransform& InTransform,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake=nullptr,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder=TEXT(""));

	static bool BakeInstancerOutputToActors_ISMC(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const FTransform& InTransform,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder=TEXT(""));

	static bool BakeInstancerOutputToActors_IAC(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData);

	static bool BakeInstancerOutputToActors_MSIC(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const FTransform& InTransform,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		FHoudiniEngineBakedActor& OutBakedActorEntry,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder="");

	static bool BakeInstancerOutputToActors_SMC(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		FHoudiniEngineBakedActor& OutBakedActorEntry,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder="");

	static bool BakeInstancerOutputToActors_LevelInstances(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		FHoudiniEngineBakedActor& OutBakedActorEntry,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor = nullptr,
		const FString& InFallbackWorldOutlinerFolder = "");

	static UStaticMesh * DuplicateStaticMeshAndCreatePackageIfNeeded(
		UStaticMesh * InStaticMesh,
		UStaticMesh * InPreviousBakeStaticMesh,
		const FHoudiniPackageParams &PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
		const FString& InTemporaryCookFolder,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap);

	static USkeletalMesh* DuplicateSkeletalMeshAndCreatePackageIfNeeded(
		USkeletalMesh* InSkeletalMesh,
		USkeletalMesh* InPreviousBakeSkeletalMesh,
		const FHoudiniPackageParams& PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
		const FString& InTemporaryCookFolder,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<USkeletalMesh*, USkeletalMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap);
	
	static USkeleton* DuplicateSkeletonAndCreatePackageIfNeeded(
		USkeleton* InSkeleton,
		USkeleton const* InPreviousBakeSkeleton,
		const FHoudiniPackageParams& PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
		const FString& InTemporaryCookFolder,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<USkeleton*, USkeleton*>& InOutAlreadyBakedSkeletonMap);

	static UPhysicsAsset* DuplicatePhysicsAssetAndCreatePackageIfNeeded(
		UPhysicsAsset * InPhysicsAsset,
		UPhysicsAsset * InPreviousBakeSkeleton,
		const FHoudiniPackageParams& PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
		const FString& InTemporaryCookFolder,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UPhysicsAsset*, UPhysicsAsset*>& InOutAlreadyBakedPhysicsAssetMap);

	static UGeometryCollection * DuplicateGeometryCollectionAndCreatePackageIfNeeded(
		UGeometryCollection * InGeometryCollection,
		UGeometryCollection * InPreviousBakeGeometryCollection,
		const FHoudiniPackageParams &PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
		const FString& InTemporaryCookFolder,
		const TMap<FSoftObjectPath, UStaticMesh*>& InOldToNewStaticMesh,
		const TMap<UMaterialInterface*, UMaterialInterface*>& InOldToNewMaterialMap,
		FHoudiniBakedObjectData& BakedObjectData);
	
	static UMaterialInterface * DuplicateMaterialAndCreatePackage(
		UMaterialInterface * Material,
		UMaterialInterface * PreviousBakeMaterial,
		const FString & SubMaterialName,
		const FHoudiniPackageParams& ObjectPackageParams,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap);


	static UFoliageType * DuplicateFoliageTypeAndCreatePackageIfNeeded(
		UFoliageType* InFoliageType,
		UFoliageType* InPreviousBakeFoliageType,
		const FHoudiniPackageParams& PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakeResults,
		const FString& InTemporaryCookFolder,
		TMap<UFoliageType*, UFoliageType*>& InOutAlreadyBakedFoliageTypes,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
		FHoudiniBakedObjectData& BakedObjectData);

	static void ReplaceDuplicatedMaterialTextureSample(
		UMaterialExpression * MaterialExpression,
		UMaterialExpression* PreviousBakeMaterialExpression,
		const FHoudiniPackageParams& PackageParams,
		FHoudiniBakedObjectData& BakedObjectData);
	
	static UTexture2D * DuplicateTextureAndCreatePackage(
		UTexture2D * Texture,
		UTexture2D* PreviousBakeTexture,
		const FString & SubTextureName,
		const FHoudiniPackageParams& PackageParams,
		FHoudiniBakedObjectData& BakedObjectData);

	// Bake a Houdini asset component (InHACToBake) based on the bInReplace and BakeOption arguments.
	// Returns true if the underlying bake function (for example, BakeHoudiniActorToActors, returns true (or a valid UObject*))
	static bool BakeHoudiniAssetComponent(
		UHoudiniAssetComponent* InHACToBake,
		FHoudiniBakeSettings & BakeSettings,
		EHoudiniEngineBakeOption InBakeOption,
		bool bInRemoveHACOutputOnSuccess);

	static bool BakeHDAToActors(
		UHoudiniAssetComponent* HoudiniAssetComponent, 
		const FHoudiniBakeSettings& BakeSettings);

	static bool BakeHDAToActors(
		UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniBakeSettings& BakeSettings,
		TArray<FHoudiniEngineBakedActor>& OutNewActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TArray<EHoudiniOutputType> * InOutputTypesToBake=nullptr,
		TArray<EHoudiniInstancerComponentType> * InInstancerComponentTypesToBake=nullptr,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder="");

	static bool BakeHoudiniOutputsToActors(
		UHoudiniAssetComponent* HoudiniAssetComponent,
		const TArray<UHoudiniOutput*>& InOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FTransform& InParentTransform,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
		TArray<FHoudiniEngineBakedActor>& OutNewActors, 
		FHoudiniBakedObjectData& BakedObjectData,
		TArray<EHoudiniOutputType> * InOutputTypesToBake=nullptr,
		TArray<EHoudiniInstancerComponentType> * InInstancerComponentTypesToBake=nullptr,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder=TEXT(""));

	static bool BakeStaticMeshOutputToActors(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex, 
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder="");

	static bool BakeSkeletalMeshOutputToActors(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor = nullptr,
		const FString& InFallbackWorldOutlinerFolder = "");

	static bool ResolvePackageParams(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		UHoudiniOutput* InOutput, 
		const FHoudiniOutputObjectIdentifier& Identifier,
		const FHoudiniOutputObject& InOutputObject,
		const bool bInHasPreviousBakeData,
		const FString& DefaultObjectName, 
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		FHoudiniPackageParams& OutPackageParams,
		FHoudiniBakedObjectData& BakedObjectData,
		const FString& InHoudiniAssetName=TEXT(""),
		const FString& InHoudiniAssetActorName=TEXT(""));

	static bool ResolvePackageParamsWithResolver(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		UHoudiniOutput* InOutput,
		const FHoudiniOutputObjectIdentifier& Identifier,
		const FHoudiniOutputObject& InOutputObject,
		const bool bInHasPreviousBakeData,
		const FString& DefaultObjectName,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		FHoudiniPackageParams& OutPackageParams,
		FHoudiniAttributeResolver & Resolver,
		FHoudiniBakedObjectData& BakedObjectData,
		const FString& InHoudiniAssetName = TEXT(""),
		const FString& InHoudiniAssetActorName = TEXT(""));
	
	static bool BakeGeometryCollectionOutputToActors(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex, 
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder=TEXT(""));

	static UUserDefinedStruct * CreateBakedUserDefinedStruct(
		UHoudiniOutput* CookedOutput,
		const FHoudiniOutputObjectIdentifier& Identifier,
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniBakedOutput& InPreviousBakedOutput,
		FHoudiniBakedOutput& InNewBakedOutput,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		FHoudiniBakedObjectData& BakedObjectData);

	static UDataTable* CreateBakedDataTable(
		UScriptStruct * UserDefinedStruct,
		const FString & ObjectName,
		UHoudiniOutput* CookedOutput,
		const FHoudiniOutputObjectIdentifier& Identifier,
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniBakedOutput& InPreviousBakedOutput,
		FHoudiniBakedOutput& InNewBakedOutput,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		FHoudiniBakedObjectData& BakedObjectData);


	static bool BakeDataTables(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor = nullptr,
		const FString& InFallbackWorldOutlinerFolder = TEXT(""));


	static UAnimSequence* CreateBakedAnimSequence(
		const FString& ObjectName,
		UHoudiniOutput* CookedOutput,
		const FHoudiniOutputObjectIdentifier& Identifier,
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniBakedOutput& InPreviousBakedOutput,
		FHoudiniBakedOutput& InNewBakedOutput,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		FHoudiniBakedObjectData& BakedObjectData);


	static bool BakeAnimSequence(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		AActor* InFallbackActor = nullptr,
		const FString& InFallbackWorldOutlinerFolder = TEXT(""));

	static bool BakeHoudiniCurveOutputToActors(
		const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutActors,
		AActor* InFallbackActor=nullptr,
		const FString& InFallbackWorldOutlinerFolder=TEXT(""));

	static bool BakeBlueprintsFromBakedActors(
		const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
		const FHoudiniBakeSettings& BakeSettings,
		const FString& InHoudiniAssetName,
		const FString& InHoudiniAssetActorName,
		const FDirectoryPath& InBakeFolder,
		TArray<FHoudiniBakedOutput>* const InNonPDGBakedOutputs,
		TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>* const InPDGBakedOutputs,
		FHoudiniBakedObjectData& BakedObjectData);
	
	static bool BakeBlueprints(UHoudiniAssetComponent* HoudiniAssetComponent, const FHoudiniBakeSettings& BakeSettings);

	static bool BakeBlueprints(
		UHoudiniAssetComponent* HoudiniAssetComponent,
		const FHoudiniBakeSettings& BakeSettings,
		FHoudiniBakedObjectData& BakedObjectData);

	static bool CopyActorContentsToBlueprint(AActor * InActor, UBlueprint * OutBlueprint, bool bInRenameComponentsWithInvalidNames=false);

	static void AddHoudiniMetaInformationToPackage(
			UPackage * Package, UObject * Object, const TCHAR * Key,
			const TCHAR * Value);

	static bool GetHoudiniGeneratedNameFromMetaInformation(
		UPackage * Package, UObject * Object, FString & HoudiniName);

	static bool DeleteBakedHoudiniAssetActor(UHoudiniAssetComponent* HoudiniAssetComponent);

	static void SaveBakedPackages(TArray<UPackage*> & PackagesToSave, bool bSaveCurrentWorld = false);

	// Look for InObjectToFind among InOutputs. Return true if found and set OutOutputIndex and OutIdentifier.
	static bool FindOutputObject(
		const UObject* InObjectToFind,
		const EHoudiniOutputType& InOutputType, 
		const TArray<UHoudiniOutput*> InOutputs,
		int32& OutOutputIndex,
		FHoudiniOutputObjectIdentifier& OutIdentifier);

	// Returns true if the object is a temporary, houdini-generated object
	static bool IsObjectTemporary(
		UObject* InObject,
		const EHoudiniOutputType& InOutputType,
		const FGuid& InComponentGuid=FGuid());

	// Returns true if the object is a temporary, houdini-generated object
	static bool IsObjectTemporary(
		UObject* InObject, 
		const EHoudiniOutputType& InOutputType,
		UHoudiniAssetComponent* InHAC);

	// Returns true if the object is a temporary, houdini-generated object
	static bool IsObjectTemporary(
		UObject* InObject,
		const EHoudiniOutputType& InOutputType,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const FString& InTemporaryCookFolder,
		const FGuid& InComponentGuid=FGuid());

	// Returns true if InObject is in InTemporaryCookFolder, 
	// or in the default Temporary cook folder from the runtime settings.
	static bool IsObjectInTempFolder(
		UObject* const InObject,
		const FString& InTemporaryCookFolder);

	// Function used to copy properties from the source Static Mesh Component to the new (baked) one
	static void CopyPropertyToNewActorAndComponent(
		AActor* NewActor,
		UStaticMeshComponent* NewSMC,
		UStaticMeshComponent* InSMC,
		bool bInCopyWorldTransform=false);

	static void CopyPropertyToNewActorAndSkeletalComponent(
		AActor* NewActor,
		USkeletalMeshComponent* NewSKC,
		USkeletalMeshComponent* InSKC,
		bool bInCopyWorldTransform = false);

	// Function used to copy properties from the source GeometryCollection Component to the new (baked) one
	static void CopyPropertyToNewGeometryCollectionActorAndComponent(
		AGeometryCollectionActor* NewActor,
		UGeometryCollectionComponent* NewGCC,
		UGeometryCollectionComponent* InGCC,
		bool bInCopyWorldTransform=false);
	
	// Finds the world/level indicated by the package path.
	// If the level doesn't exists, it will be created.
	// If InLevelPath is empty, outputs the editor world and current level
	// Returns true if the world/level were found, false otherwise
	static bool FindOrCreateDesiredLevelFromLevelPath(
		const FString& InLevelPath,
		ULevel*& OutDesiredLevel,
		UWorld*& OutDesiredWorld,
		bool& OutCreatedPackage);

	// Finds the actor indicated by InBakeActorName in InLevel.
	// Returns false if any input was invalid (InLevel is invalid for example), true otherwise
	// If an actor was found OutActor is set
	// If bInNoPendingKillActors is true, then if an actor called InBakeActorName is found but is pending kill, then
	// it is not set in OutActor
	// If bRenamePendingKillActor is true, then if a pending kill actor call InBakeActorName is found it is renamed
	// (uniquely) with a _Pending_Kill suffix (regardless of bInNoPendingKillActors).
	static bool FindBakedActor(
		const FString& InBakeActorName,
		const TSubclassOf<AActor>& InBakeActorClass,
		ULevel* InLevel,
		AActor*& OutActor,
		bool bInNoPendingKillActors=true,
		bool bRenamePendingKillActor=true);

	// Helper that determines the desired bake actor name with unreal_bake_actor attribute, falling
	// back to InDefaultActorName if the attribute is not set.
	// If unreal_bake_actor is set, we look for such in InLevel, and use it *if* it is present in InAlLBakedOutputs.
	// Otherwise if we are baking in replace mode, and the previous bake actor is available and in InLevel, return it
	// as OutFoundActor. Otherwise return InFallbackActor as OutFoundActor.
	// bOutHasBakeActorName indicates if the output has the unreal_bake_actor attribute set.
	// OutFoundActor is the actor that was found (if one was found)
	static void FindUnrealBakeActor(
		const FHoudiniOutputObject& InOutputObject,
		const FHoudiniBakedOutputObject& InBakedOutputObject,
		const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
		ULevel* InLevel,
		FName InDefaultActorName,
		const FHoudiniBakeSettings& BakeSettings,
		AActor* InFallbackActor,
		AActor*& OutFoundActor,
		bool& bOutHasBakeActorName,
		FName& OutBakeActorName);

	// Try to find an actor that we can use for baking.
	// If the requested actor could not be found, then `OutWorld` and `OutLevel`
	// should be used to spawn the new bake actor.
	// @returns AActor* if found. Otherwise, returns nullptr.
	static AActor* FindExistingActor_Bake(
		UWorld* InWorld,
		UHoudiniOutput* InOutput,
		const FString& InActorName,
		const FString& InPackagePath,
		UWorld*& OutWorld,
		ULevel*& OutLevel,
		bool& bCreatedPackage);

	// Remove a previously baked actor
	static bool RemovePreviouslyBakedActor(
		AActor* InNewBakedActor,
		ULevel* InLevel,
		const FHoudiniPackageParams& InPackageParams);

	static bool RemovePreviouslyBakedComponent(UActorComponent* InComponent);

	// Get the world outliner folder path for output generated by InOutputOwner
	static FName GetOutputFolderPath(UObject* InOutputOwner);

	static void RenameAsset(UObject* InAsset, const FString& InNewName, bool bMakeUniqueIfNotUnique=true);
	
	// Helper function for renaming and relabelling an actor
	static void RenameAndRelabelActor(AActor* InActor, const FString& InNewName, bool bMakeUniqueIfNotUnique=true);
	
	// Helper function for duplicating Static Meshes
	//static UStaticMesh* DuplicateStaticMesh(UStaticMesh* SourceStaticMesh, UObject* Outer, const FName Name);

	// Start: PDG Baking

	// Detach InActor from its parent, and rename to InNewName (attaches a numeric suffix to make it unique via
	// MakeUniqueObjectName). Place it in the world outliner folder InFolderPath.
	static bool DetachAndRenameBakedPDGOutputActor(AActor* InActor, const FString& InNewName, const FName& InFolderPath);
	
	static bool BakePDGWorkResultObject(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNode* InNode,
		int32 InWorkResultArrayIndex,
		int32 InWorkResultObjectArrayIndex,
		const FHoudiniBakeSettings& BakeSettings,
		bool bInBakeToWorkResultActor,
		bool bInIsAutoBake,
		const TArray<FHoudiniEngineBakedActor>& InBakedActors,
		TArray<FHoudiniEngineBakedActor>& OutBakedActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TArray<EHoudiniOutputType> * InOutputTypesToBake=nullptr,
		TArray<EHoudiniInstancerComponentType> * InInstancerComponentTypesToBake=nullptr,
		const FString& InFallbackWorldOutlinerFolder="");

	// Checks if auto-bake is enabled on InPDGAssetLink, and if it is, calls BakePDGWorkResultObject.
	static void CheckPDGAutoBakeAfterResultObjectLoaded(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNode* InNode,
		int32 InWorkItemHAPIIndex,
		int32 InWorkItemResultInfoIndex);

	//FROSTY_BEGIN_CHANGE
	// Checks if auto-bake is enabled on InPDGAssetLink, and if it is, calls BakePDGWorkResultObject.
	static void PDGAutoBakeAfterResultObjectLoaded(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNode* InNode,
		int32 InWorkItemHAPIIndex,
		int32 InWorkItemResultInfoIndex,
		TArray<FHoudiniEngineBakedActor>& OutBakedActors);
	//FROSTY_END_CHANGE

	// Bake PDG output. This bakes all assets from all work items in the specified InNode (FTOPNode).
	// It uses the existing output actors in the level, but breaks any links from these actors to the PDG link and
	// moves the actors out of the parent Folder/ detaches from the parent PDG output actor.
	static bool BakePDGTOPNodeOutputsKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNode* InNode,
		bool bInBakeForBlueprint,
		bool bInIsAutoBake,
		const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
		TArray<FHoudiniEngineBakedActor>& OutBakedActors,
		FHoudiniBakedObjectData& BakedObjectData);

	// Helper function to bake only a specific PDG TOP node's outputs to actors.
	static bool BakePDGTOPNodeOutputsKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNode* InTOPNode,
		bool bInIsAutoBake,
		const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
		bool bInRecenterBakedActors,
		//FROSTY_BEGIN_CHANGE
		TArray<FHoudiniEngineBakedActor>& OutBakedActors);
		//FROSTY_END_CHANGE

	// Bake PDG output. This bakes all assets from all work items in the specified TOP network.
	// It uses the existing output actors in the level, but breaks any links
	// from these actors to the PDG link and moves the actors out of the parent Folder/ detaches from the parent
	// PDG output actor.
	static bool BakePDGTOPNetworkOutputsKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNetwork* InNetwork,
		bool bInBakeForBlueprint,
		bool bInIsAutoBake,
		const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
		TArray<FHoudiniEngineBakedActor>& OutBakedActors,
		FHoudiniBakedObjectData& BakedObjectData);

	// Bake PDG output. This bakes assets from TOP networks and nodes according to
	// InPDGAssetLink->PDGBakeSelectionOption. It uses the existing output actors in the level, but breaks any links
	// from these actors to the PDG link and moves the actors out of the parent Folder/ detaches from the parent
	// PDG output actor.
	static bool BakePDGAssetLinkOutputsKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink, 
		EPDGBakeSelectionOption InBakeSelectionOption, 
		EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
		bool bInRecenterBakedActors);

	static bool BakePDGAssetLinkOutputsKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		EPDGBakeSelectionOption InBakeSelectionOption,
		EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
		bool bInRecenterBakedActors,
		FHoudiniBakedObjectData& BakedObjectData,
		TArray<FHoudiniEngineBakedActor> & BakedActors);

	// Bake PDG output. This bakes all supported assets from all work items in the specified InNode (FTOPNode).
	// It duplicates the output actors and bakes them to blueprints. Assets that were baked are removed from
	// PDG output actors.
	static bool BakePDGTOPNodeBlueprints(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNode* InNode,
		bool bInIsAutoBake,
		EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
		bool bInRecenterBakedActors,
		FHoudiniBakedObjectData& BakedObjectData);

	// Helper to bake only a specific PDG TOP node's outputs to blueprint(s).
	static bool BakePDGTOPNodeBlueprints(
		UHoudiniPDGAssetLink* InPDGAssetLink, 
		UTOPNode* InTOPNode, 
		bool bInIsAutoBake, 
		const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
		bool bInRecenterBakedActors);
	
	// Bake PDG output. This bakes all supported assets from all work items in the specified TOP network.
	// It duplicates the output actors and bakes them to blueprints. Assets that were baked are removed from
	// PDG output actors.
	static bool BakePDGTOPNetworkBlueprints(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		UTOPNetwork* InNetwork,
		EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode,
		bool bInRecenterBakedActors,
		FHoudiniBakedObjectData& BakedObjectData);

	// Bake PDG output. This bakes assets from TOP networks and nodes according to
	// InPDGAssetLink->PDGBakeSelectionOption. It duplicates the output actors and bakes them to blueprints. Assets
	// that were baked are removed from PDG output actors.
	static bool BakePDGAssetLinkBlueprints(
		UHoudiniPDGAssetLink* InPDGAssetLink, 
		const EPDGBakeSelectionOption InBakeSelectionOption, 
		const EPDGBakePackageReplaceModeOption InPDGBakePackageReplaceMode, 
		bool bInRecenterBakedActors);

	// End: PDG Baking

public:

	// Find the HGPO with matching identifier. Returns true if the HGPO was found.
	static bool FindHGPO(
		const FHoudiniOutputObjectIdentifier& InIdentifier,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		FHoudiniGeoPartObject const*& OutHGPO);

	// Set OutBakeName to the resolved output name of InMeshOutputObject / InObject. OutBakeName is set to the object's
	// BakeName (the BakeName on the InMeshOutputObject, or if that is not set, the custom part name or finally the
	// package name.
	static void GetTemporaryOutputObjectBakeName(
		const UObject* InObject,
		const FHoudiniOutputObject& InMeshOutputObject,
		FString& OutBakeName);

	// Look for InObject in InAllOutputs. If found the function returns true and OutBakeName is set to the object's
	// BakeName (the BakeName on the OutputObject, or if that is not set, the custom part name or finally the package
	// name.
	static bool GetTemporaryOutputObjectBakeName(
		const UObject* InObject,
		EHoudiniOutputType InOutputType,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		FString& OutBakeName);

	// Checks if InHoudiniAssetComponent has any current proxy mesh. Refines if it possible. Returns true
	// if baking can continue, false otherwise. If the component has a proxy, but no cook data, then false is 
	// returned, the component is set to recook without a proxy and with bake after cook, and bOutNeedsReCook is set
	// to true.
	// bInReplace and BakeOption represents the baking settings to use if a delayed bake (post-cook) needs to be triggered.
	static bool CheckForAndRefineHoudiniProxyMesh(
		UHoudiniAssetComponent* InHoudiniAssetComponent,
		bool bInReplacePreviousBake,
		EHoudiniEngineBakeOption BakeOption,
		bool bInRemoveHACOutputOnSuccess,
		bool bInRecenterBakedActors,
		bool& bOutNeedsReCook);

	// Position InActor at its bounding box center (keep components' world location) 
	static void CenterActorToBoundingBoxCenter(AActor* InActor);

	// Position each of the actors in InActors at its bounding box center (keep components' world location) 
	static void CenterActorsToBoundingBoxCenter(const TArray<AActor*>& InActors);

	// Helper to get or optionally create a RootComponent for an actor
	static USceneComponent* GetActorRootComponent(
		AActor* InActor, bool bCreateIfMissing=true, EComponentMobility::Type InMobilityIfCreated=EComponentMobility::Static);

	// Helper function to return a unique object name if the given is already in use
	static FString MakeUniqueObjectNameIfNeeded(UObject* InOuter, const UClass* InClass, const FString& InName, UObject* InObjectThatWouldBeRenamed=nullptr);

	// Helper for getting the actor folder path for the world outliner, based unreal_bake_outliner_folder
	static FName GetOutlinerFolderPath(const FHoudiniAttributeResolver& Resolver, FName InDefaultFolder);

	// Helper for setting the actor folder path in the world outliner
	static bool SetOutlinerFolderPath(AActor* InActor, FName InDefaultFolder);

	// Helper for destroying previous bake components/actors
	static uint32 DestroyPreviousBakeOutput(
		FHoudiniBakedOutputObject& InBakedOutputObject,
		bool bInDestroyBakedComponent,
		bool bInDestroyBakedInstancedActors,
		bool bInDestroyBakedInstancedComponents);

	static UMaterialInterface * BakeSingleMaterialToPackage(
		UMaterialInterface * InOriginalMaterial, 
		const FHoudiniPackageParams & PackageParams, 
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap);

	// Returns the AActor class with classname InActorClassName, or nullptr if InActorClassName is none or not a valid
	// actor class name.
	static UClass* GetBakeActorClassOverride(const FName& InActorClassName);

	// Returns the AActor class with classname read from the unreal_bake_actor_class attribute, or nullptr if
	// attribute is not set or is invalid is none or not a valid actor class name.
	static UClass* GetBakeActorClassOverride(const FHoudiniOutputObject& InOutputObject);
	
	// Helper function for getting the appropriate ActorFactory by unreal_bake_actor_class attribute, failing that by
	// the specified ActorFactoryClass, and lastly by the asset that would be spawned
	// OutActorClass is set to the Actor UClass specified by InActorClassName or null if InActorClassName is invalid.
	static UActorFactory* GetActorFactory(
		const FName& InActorClassName,
		const FHoudiniBakeSettings& BakeSettings,
		TSubclassOf<AActor>& OutActorClass, 
		const TSubclassOf<UActorFactory>& InFactoryClass=nullptr, 
		UObject* InAsset=nullptr);

	// Helper function for getting the appropriate ActorFactory by unreal_bake_actor_class attribute (read from
	// InOutputObject.CachedAttributes), failing that by the specified ActorFactoryClass, and lastly by the asset that
	// would be spawned
	// OutActorClass is set to the Actor UClass specified by unreal_bake_actor_class or null if unreal_bake_actor_class
	// is not set or invalid.
	static UActorFactory* GetActorFactory(
		const FHoudiniOutputObject& InOutputObject, 
		const FHoudiniBakeSettings& BakeSettings, 
		TSubclassOf<AActor>& OutActorClass, 
		const TSubclassOf<UActorFactory>& InFactoryClass=nullptr, 
		UObject* InAsset=nullptr);

	// Helper function that spawns an actor via InActorFactory. 
	static AActor* SpawnBakeActor(
		UActorFactory* InActorFactory, 
		UObject* InAsset, 
		ULevel* InLevel,
		const FHoudiniBakeSettings& BakeSettings,
		const FTransform& InTransform, 
		const UHoudiniAssetComponent * InHAC, 
		const TSubclassOf<AActor>& InActorClass=nullptr, 
		const FActorSpawnParameters& InSpawnParams=FActorSpawnParameters());

	// Called by SpawnBakeActor after the actor was successfully spawned. Used to copy any settings we need from the
	// HAC or its owner to the spawned actor and/or its root component.
	static void PostSpawnBakeActor(AActor* const InSpawnedActor, UHoudiniAssetComponent const * const InHAC);

	// Helper for baking a static mesh output to actors. Returns true if anything was baked. If the mesh had an
	// associated component and was baked to an actor then bOutBakedToActor is set to true and OutBakedActorEntry
	// is populated. Some meshes, such as invisible colliders, are not baked to actors, the mesh asset itself is
	// just baked. In that case bOutBakedToActor is false and OutBakedActorEntry is not populated.
	static bool BakeStaticMeshOutputObjectToActor(
		const UHoudiniAssetComponent* InHoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		const FHoudiniOutputObjectIdentifier& InIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InTempCookFolder,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		AActor* InFallbackActor,
		const FString& InFallbackWorldOutlinerFolder,
		const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface *, UMaterialInterface *>& InOutAlreadyBakedMaterialsMap,
		FHoudiniBakedObjectData& BakedObjectData,
		bool& bOutBakedToActor,
		FHoudiniEngineBakedActor& OutBakedActorEntry);

	// Skeletal Mesh Version
	static bool BakeSkeletalMeshOutputObjectToActor(
		const UHoudiniAssetComponent* InHoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		const FHoudiniOutputObjectIdentifier& InIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		const TArray<FHoudiniGeoPartObject>& InHGPOs,
		FHoudiniEngineBakeState& InBakeState,
		const FDirectoryPath& InTempCookFolder,
		const FDirectoryPath& InBakeFolder,
		const FHoudiniBakeSettings& BakeSettings,
		AActor* InFallbackActor,
		const FString& InFallbackWorldOutlinerFolder,
		const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
		TMap<USkeletalMesh*, USkeletalMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		FHoudiniBakedObjectData& BakedObjectData,
		bool& bOutBakedToActor,
		FHoudiniEngineBakedActor& OutBakedActorEntry);

	TArray<UStaticMesh*> GetBakes(const UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		FHoudiniBakedOutputObject& InBakedOutputObject,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& BakeResults,
		FHoudiniEngineBakedActor& OutBakeActorEntry,
		FHoudiniBakedObjectData& BakedObjectData,
		TMap<UStaticMesh*, UStaticMesh*>& InOutAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap);

	static bool BakeFoliageTypes(
		TMap<UFoliageType*, UFoliageType*> & FoliageMap,
	    UHoudiniAssetComponent* HoudiniAssetComponent,
		int32 InOutputIndex,
		FHoudiniEngineBakeState& InBakeState,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& BakeResults,
		const TMap<UStaticMesh*, UStaticMesh*>& InAlreadyBakedStaticMeshMap,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		FHoudiniBakedObjectData& BakedObjectData);

	static void BakeAllFoliageTypes(
		UHoudiniAssetComponent* HoudiniAssetComponent,
		const TMap<UStaticMesh*, UStaticMesh*>& AlreadyBakedStaticMeshMap,
		FHoudiniEngineBakeState& InBakeState,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		const FDirectoryPath& InBakeFolder,
		const FDirectoryPath& InTempCookFolder,
		const FHoudiniBakeSettings& BakeSettings,
		const TArray<FHoudiniEngineBakedActor>& BakeResults,
		TMap<UMaterialInterface*, UMaterialInterface*>& InOutAlreadyBakedMaterialsMap,
		FHoudiniBakedObjectData& BakedObjectData);

	static void RemoveBakedFoliageInstances(UHoudiniAssetComponent* HoudiniAssetComponent, TArray<FHoudiniBakedOutput>& InBakedOutputs);

	static void RemoveBakedLevelInstances(
		UHoudiniAssetComponent* HoudiniAssetComponent, 
		TArray<FHoudiniBakedOutput>& InBakedOutputs,
		const FHoudiniBakeSettings& BakeSettings);

	static void DeleteBakedDataTableObjects(TArray<FHoudiniBakedOutput>& InBakedOutputs);

	static UUserDefinedStruct * DuplicateUserDefinedStruct(UUserDefinedStruct* OldStruct, UPackage* Pacakge, FString& PackageName);

};
