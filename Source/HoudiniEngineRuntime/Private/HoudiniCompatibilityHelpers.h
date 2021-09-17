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
#include "UObject/ObjectMacros.h"

#include "HoudiniAsset.h"

#include "Components/PrimitiveComponent.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

#include "HoudiniCompatibilityHelpers.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USplineComponent;
class ALandscapeProxy;
class UMaterialInterface;
class UHoudiniInput;
class UHoudiniParameter;
class UHoudiniHandleComponent;
class UHoudiniSplineComponent;
class UHoudiniInstancedActorComponent;
class UHoudiniMeshSplitInstancerComponent;
class UFoliageType_InstancedStaticMesh;


struct FHoudiniGeoPartObject;


struct FHoudiniGeoPartObject_V1
{
public:

	void Serialize(FArchive & Ar);

	FHoudiniGeoPartObject ConvertLegacyData();

	/** Return hash value for this object, used when using this object as a key inside hashing containers. **/
	uint32 GetTypeHash() const;

	/** Transform of this geo part object. **/
	FTransform TransformMatrix;

	/** Name of associated object. **/
	FString ObjectName;

	/** Name of associated part. **/
	FString PartName;

	/** Name of group which was used for splitting, empty if there's none. **/
	FString SplitName;

	/** Name of the instancer material, if available. **/
	FString InstancerMaterialName;

	/** Name of attribute material, if available. **/
	FString InstancerAttributeMaterialName;

	/** Id of corresponding HAPI Asset. **/
	//HAPI_NodeId AssetId;
	int AssetId;

	/** Id of corresponding HAPI Object. **/
	//HAPI_NodeId ObjectId;
	int  ObjectId;

	/** Id of corresponding HAPI Geo. **/
	//HAPI_NodeId GeoId;
	int GeoId;

	/** Id of corresponding HAPI Part. **/
	//HAPI_PartId PartId;
	int PartId;

	/** Id of a split. In most cases this will be 0. **/
	int32 SplitId;

	/** Path to the corresponding node */
	mutable FString NodePath;

	/** Flags used by geo part object. **/
	union
	{
		struct
		{
			/* Is set to true when referenced object is visible. This is typically used by instancers. **/
			uint32 bIsVisible : 1;

			/** Is set to true when referenced object is an instancer. **/
			uint32 bIsInstancer : 1;

			/** Is set to true when referenced object is a curve. **/
			uint32 bIsCurve : 1;

			/** Is set to true when referenced object is editable. **/
			uint32 bIsEditable : 1;

			/** Is set to true when geometry has changed. **/
			uint32 bHasGeoChanged : 1;

			/** Is set to true when referenced object is collidable. **/
			uint32 bIsCollidable : 1;

			/** Is set to true when referenced object is collidable and is renderable. **/
			uint32 bIsRenderCollidable : 1;

			/** Is set to true when referenced object has just been loaded. **/
			uint32 bIsLoaded : 1;

			/** Unused flags. **/
			uint32 bPlaceHolderFlags : 3;

			/** Is set to true when referenced object has been loaded during transaction. **/
			uint32 bIsTransacting : 1;

			/** Is set to true when referenced object has a custom name. **/
			uint32 bHasCustomName : 1;

			/** Is set to true when referenced object is a box. **/
			uint32 bIsBox : 1;

			/** Is set to true when referenced object is a sphere. **/
			uint32 bIsSphere : 1;

			/** Is set to true when instancer material is available. **/
			uint32 bInstancerMaterialAvailable : 1;

			/** Is set to true when referenced object is a volume. **/
			uint32 bIsVolume : 1;

			/** Is set to true when instancer attribute material is available. **/
			uint32 bInstancerAttributeMaterialAvailable : 1;

			/** Is set when referenced object contains packed primitive instancing */
			uint32 bIsPackedPrimitiveInstancer : 1;

			/** Is set to true when referenced object is a UCX collision geo. **/
			uint32 bIsUCXCollisionGeo : 1;

			/** Is set to true when referenced object is a rendered UCX collision geo. **/
			uint32 bIsSimpleCollisionGeo : 1;

			/** Is set to true when new collision geo has been generated **/
			uint32 bHasCollisionBeenAdded : 1;

			/** Is set to true when new sockets have been added **/
			uint32 bHasSocketBeenAdded : 1;

			/** unused flag space is zero initialized */
			uint32 UnusedFlagsSpace : 14;
		};

		uint32 HoudiniGeoPartObjectFlagsPacked;
	};

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniGeoPartObjectVersion;
};

/** Function used by hashing containers to create a unique hash for this type of object. **/
uint32 GetTypeHash(const FHoudiniGeoPartObject_V1 & HoudiniGeoPartObject);

/** Serialization function. **/
FArchive& operator<<(FArchive & Ar, FHoudiniGeoPartObject_V1& HoudiniGeoPartObject);

/** Functor used to sort geo part objects. **/
struct FHoudiniGeoPartObject_V1SortPredicate
{
	bool operator()(const FHoudiniGeoPartObject_V1& A, const FHoudiniGeoPartObject_V1& B) const;
};


struct FHoudiniAssetInputOutlinerMesh_V1
{
	/** Serialization. **/
	void Serialize(FArchive & Ar);

	/** Update the Actor pointer from the store Actor path/name **/
	bool TryToUpdateActorPtrFromActorPathName(UWorld* InWorld);

	/** Temporary variable holding serialization version. **/
	uint32 HoudiniAssetParameterVersion;

	/** Selected Actor. **/
	TWeakObjectPtr<AActor> ActorPtr = nullptr;

	/** Selected Actor's path, used to find the actor back after loading. **/
	FString ActorPathName = TEXT("NONE");

	/** Selected mesh's component, for reference. **/
	UStaticMeshComponent * StaticMeshComponent = nullptr;

	/** The selected mesh. **/
	UStaticMesh * StaticMesh = nullptr;

	/** Spline Component **/
	USplineComponent * SplineComponent = nullptr;

	/** Number of CVs used by the spline component, used to detect modification **/
	int32 NumberOfSplineControlPoints = -1;

	/** Transform of the UnrealSpline CVs, used to detect modification of the spline (Rotation/Scale) **/
	TArray<FTransform> SplineControlPointsTransform;

	/** Spline Length, used to detect modification of the spline.. **/
	float SplineLength = -1.0f;

	/** Spline resolution used to generate the asset, used to detect setting modification **/
	float SplineResolution = -1.0f;

	/** Actor transform used to see if the transfrom changed since last marshal into Houdini. **/
	FTransform ActorTransform;

	/** Component transform used to see if the transform has changed since last marshalling **/
	FTransform ComponentTransform;

	/** Mesh's input asset id. **/
	//HAPI_NodeId AssetId = -1;
	int AssetId = -1;

	/** TranformType used to generate the asset **/
	int32 KeepWorldTransform = 2;

	/** Path Materials assigned on the SMC **/
	TArray<FString> MeshComponentsMaterials;

	/** If the world In is a ISM, index of this instance **/
	uint32 InstanceIndex = -1;
};

/** Serialization function. **/
FArchive & operator<<(FArchive & Ar, FHoudiniAssetInputOutlinerMesh_V1& HoudiniAssetInputOutlinerMesh);

/*
UCLASS(EditInlineNew, config = Engine)
class UHoudiniAsset_V1
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;
};
*/

UCLASS()
class UHoudiniAssetParameter : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer);

	void CopyLegacyParameterData(UHoudiniParameter* InNewParm);

	/** Name of this parameter. **/
	FString ParameterName;

	/** Label of this parameter. **/
	FString ParameterLabel;

	/** Node this parameter belongs to. **/
	int NodeId;

	/** Id of this parameter. **/
	int ParmId;

	/** Id of parent parameter, -1 if root is parent. **/
	int ParmParentId;

	/** Child index within its parent parameter. **/
	int32 ChildIndex;

	/** Tuple size - arrays. **/
	int32 TupleSize;

	/** Internal HAPI cached value index. **/
	int32 ValuesIndex;

	/** The multiparm instance index. **/
	int32 MultiparmInstanceIndex;

	/** The parameter's help, to be used as a tooltip **/
	FString ParameterHelp;

	/** Flags used by this parameter. **/
	union
	{
		struct
		{
			/** Is set to true if this parameter is spare, that is, created by Houdini Engine only. **/
			uint32 bIsSpare : 1;

			/** Is set to true if this parameter is disabled. **/
			uint32 bIsDisabled : 1;

			/** Is set to true if value of this parameter has been changed by user. **/
			uint32 bChanged : 1;

			/** Is set to true when parameter's slider (if it has one) is being dragged. Transient. **/
			uint32 bSliderDragged : 1;

			/** Is set to true if the parameter is a multiparm child parameter. **/
			uint32 bIsChildOfMultiparm : 1;

			/** Is set to true if this parameter is a Substance parameter. **/
			uint32 bIsSubstanceParameter : 1;

			/** Is set to true if this parameter is a multiparm **/
			uint32 bIsMultiparm : 1;
		};

		uint32 HoudiniAssetParameterFlagsPacked;
	};

	/** Temporary variable holding parameter serialization version. **/
	uint32 HoudiniAssetParameterVersion;
};

UCLASS()
class UHoudiniAssetParameterButton : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;
};

UCLASS()
class UHoudiniAssetParameterChoice : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Choice values for this property. **/
	TArray<FString> StringChoiceValues;

	/** Choice labels for this property. **/
	TArray<FString> StringChoiceLabels;

	/** Value of this property. **/
	FString StringValue;

	/** Current value for this property. **/
	int32 CurrentValue;

	/** Is set to true when this choice list is a string choice list. **/
	bool bStringChoiceList;
};

UCLASS()
class UHoudiniAssetParameterColor : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Color for this property. **/
	FLinearColor Color;
};

UCLASS()
class UHoudiniAssetParameterFile : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Values of this property. **/
	TArray< FString > Values;

	/** Filters of this property. **/
	FString Filters;

	/** Is the file parameter read-only? **/
	bool IsReadOnly;
};

UCLASS()
class UHoudiniAssetParameterFloat : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Values of this property. **/
	TArray< float > Values;

	/** Min and Max values for this property. **/
	float ValueMin;
	float ValueMax;

	/** Min and Max values for UI for this property. **/
	float ValueUIMin;
	float ValueUIMax;

	/** Unit for this property **/
	FString ValueUnit;

	/** Do we have the noswap tag? **/
	bool NoSwap;
};

UCLASS()
class UHoudiniAssetParameterFolder : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;
};

UCLASS()
class UHoudiniAssetParameterFolderList : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;
};

UCLASS()
class UHoudiniAssetParameterInt : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Values of this property. **/
	TArray< int32 > Values;

	/** Min and Max values for this property. **/
	int32 ValueMin;
	int32 ValueMax;

	/** Min and Max values for UI for this property. **/
	int32 ValueUIMin;
	int32 ValueUIMax;

	/** Unit for this property **/
	FString ValueUnit;
};

UCLASS()
class UHoudiniAssetParameterLabel : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;
};

UCLASS()
class UHoudiniAssetParameterMultiparm : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Value of this property. **/
	int32 MultiparmValue;
};

UCLASS()
class UHoudiniAssetParameterRamp : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	void CopyLegacyParameterData(UHoudiniParameter* InNewParm);

	//! Curves which are being edited.
	UCurveFloat * HoudiniAssetParameterRampCurveFloat;
	UCurveLinearColor * HoudiniAssetParameterRampCurveColor;

	//! Set to true if this ramp is a float ramp. Otherwise is considered a color ramp.
	bool bIsFloatRamp;
};

UCLASS()
class UHoudiniAssetParameterSeparator : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;
};

UCLASS()
class UHoudiniAssetParameterString : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Values of this property. **/
	TArray< FString > Values;
};

UCLASS()
class UHoudiniAssetParameterToggle : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniParameter* ConvertLegacyData(UObject* Outer) override;

	/** Values of this property. **/
	TArray< int32 > Values;
};

UCLASS()
class UHoudiniAssetComponentMaterials_V1 : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;
	
	/** Material assignments. **/
	TMap<FString, UMaterialInterface *> Assignments;

	/** Material replacements. **/
	TMap<FHoudiniGeoPartObject_V1, TMap<FString, UMaterialInterface *>> Replacements;
};

UCLASS()
class UHoudiniHandleComponent_V1 : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	bool UpdateFromLegacyData(UHoudiniHandleComponent* NewHC);

	//virtual UHoudiniHandleComponent* ConvertLegacyData(UObject* Outer);

	UHoudiniAssetParameterFloat* XFormParams[9];
	int32 XFormParamsTupleIndex[9];

	UHoudiniAssetParameterChoice* RSTParm;
	int32 RSTParmTupleIdx;

	UHoudiniAssetParameterChoice* RotOrderParm;
	int32 RotOrderParmTupleIdx;
};

UCLASS()
class UHoudiniSplineComponent_V1 : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	virtual UHoudiniSplineComponent* ConvertLegacyData(UObject* Outer);

	bool UpdateFromLegacyData(UHoudiniSplineComponent* NewSpline);

	/** Corresponding geo part object. **/
	FHoudiniGeoPartObject_V1 HoudiniGeoPartObject;

	/** List of points composing this curve. **/
	TArray<FTransform> CurvePoints;

	/** List of refined points used for drawing. **/
	TArray<FVector> CurveDisplayPoints;

	/** Type of this curve. **/
	// 0 Polygon 1 Nurbs 2 Bezier
	uint8 CurveType;

	/** Method used for this curve. **/       
	// 0 CVs, 1 Breakpoints, 2 Freehand
	uint8 CurveMethod;

	/** Whether this spline is closed. **/
	bool bClosedCurve;
};

UCLASS()
class UHoudiniAssetInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	UHoudiniInput* ConvertLegacyInput(UObject* Outer);

	// Input type:
	// 0 GeometryInput
	// 1 AssetInput
	// 2 CurveInput
	// 3 LandscapeInput
	// 4 WorldInput
	// 5 SkeletonInput
	uint8 ChoiceIndex;

	/** Value of choice option. **/
	FString ChoiceStringValue;

	/** Index of this input. **/
	int32 InputIndex;

	/** Objects used for geometry input. **/
	TArray<UObject *> InputObjects;

	/** Houdini spline component which is used for curve input. **/
	UHoudiniSplineComponent * InputCurve;

	/** Houdini asset component pointer of the input asset (actor). **/
	UHoudiniAssetComponent_V1 * InputAssetComponent;

	/** Landscape actor used for input. **/
	TSoftObjectPtr<ALandscapeProxy> InputLandscapeProxy;

	/** List of selected meshes and actors from the World Outliner. **/
	TArray<FHoudiniAssetInputOutlinerMesh_V1> InputOutlinerMeshArray;

	/** Parameters used by a curve input asset. **/
	TMap<FString, UHoudiniAssetParameter *> InputCurveParameters;

	float UnrealSplineResolution;

	/** Array containing the transform corrections for the assets in a geometry input **/
	TArray<FTransform> InputTransforms;

	/** Transform used by the input landscape **/
	FTransform InputLandscapeTransform;

	/** Flags used by this input. **/
	union
	{
		struct
		{
			/** Is set to true when static mesh used for geometry input has changed. **/
			uint32 bStaticMeshChanged : 1;

			/** Is set to true when choice switches to curve mode. **/
			uint32 bSwitchedToCurve : 1;

			/** Is set to true if this parameter has been loaded. **/
			uint32 bLoadedParameter : 1;

			/** Is set to true if the asset input is actually connected inside Houdini. **/
			uint32 bInputAssetConnectedInHoudini : 1;

			/** Is set to true when landscape input is set to selection only. **/
			uint32 bLandscapeExportSelectionOnly : 1;

			/** Is set to true when landscape curves are to be exported. **/
			uint32 bLandscapeExportCurves : 1;

			/** Is set to true when the landscape is to be exported as a mesh, not just points. **/
			uint32 bLandscapeExportAsMesh : 1;

			/** Is set to true when materials are to be exported. **/
			uint32 bLandscapeExportMaterials : 1;

			/** Is set to true when lightmap information export is desired. **/
			uint32 bLandscapeExportLighting : 1;

			/** Is set to true when uvs should be exported in [0,1] space. **/
			uint32 bLandscapeExportNormalizedUVs : 1;

			/** Is set to true when uvs should be exported for each tile separately. **/
			uint32 bLandscapeExportTileUVs : 1;

			/** Is set to true when being used as an object-path parameter instead of an input */
			uint32 bIsObjectPathParameter : 1;

			/** Is set to true when this input's Transform Type is set to NONE, 2 will use the input's default value **/
			uint32 bKeepWorldTransform : 2;

			/** Is set to true when the landscape is to be exported as a heightfield **/
			uint32 bLandscapeExportAsHeightfield : 1;

			/** Is set to true when the automatic selection of landscape component is active **/
			uint32 bLandscapeAutoSelectComponent : 1;

			/** Indicates that the geometry must be packed before merging it into the input **/
			uint32 bPackBeforeMerge : 1;

			/** Indicates that all LODs in the input should be marshalled to Houdini **/
			uint32 bExportAllLODs : 1;

			/** Indicates that all sockets in the input should be marshalled to Houdini **/
			uint32 bExportSockets : 1;

			/** Indicates that the landscape input's source landscape should be updated instead of creating a new component **/
			uint32 bUpdateInputLandscape : 1;
		};

		uint32 HoudiniAssetInputFlagsPacked;
	};
};

UCLASS()
class UHoudiniAssetInstanceInput : public UHoudiniAssetParameter
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	/** List of fields created by this instance input. **/
	TArray<UHoudiniAssetInstanceInputField *> InstanceInputFields;

	/** Corresponding geo part object. **/
	FHoudiniGeoPartObject_V1 HoudiniGeoPartObject;

	/** Id of an object to instance. **/
	int ObjectToInstanceId;

public:
	/** Flags used by this input. **/
	union FHoudiniAssetInstanceInputFlags
	{
		struct
		{
			/** Set to true if this is an attribute instancer. **/
			uint32 bIsAttributeInstancer : 1;

			/** Set to true if this attribute instancer uses overrides. **/
			uint32 bAttributeInstancerOverride : 1;

			/** Set to true if this is a packed primitive instancer **/
			uint32 bIsPackedPrimitiveInstancer : 1;

			/** Set to true if this is a split mesh instancer */
			uint32 bIsSplitMeshInstancer : 1;
		};

		uint32 HoudiniAssetInstanceInputFlagsPacked;
	};
	FHoudiniAssetInstanceInputFlags Flags;
};
 
UCLASS()
class UHoudiniAssetInstanceInputField : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	/** Original object used by the instancer. **/
	UObject* OriginalObject;

	/** Currently used Objects */
	TArray< UObject* > InstancedObjects;

	/** Used instanced actor component. **/
	TArray< USceneComponent * > InstancerComponents;

	/** Flags used by this input field. **/
	uint32 HoudiniAssetInstanceInputFieldFlagsPacked;

	/** Corresponding geo part object. **/
	FHoudiniGeoPartObject_V1 HoudiniGeoPartObject;

	/** Rotation offset for instanced component. **/
	TArray< FRotator > RotationOffsets;

	/** Scale offset for instanced component. **/
	TArray< FVector > ScaleOffsets;

	/** Whether to scale linearly for all fields. **/
	TArray< bool > bScaleOffsetsLinearlyArray;

	/** Transforms, one for each instance. **/
	TArray< FTransform > InstancedTransforms;

	/** Assignment of Transforms to each variation **/
	TArray< TArray< FTransform > > VariationTransformsArray;

	/** Color overrides, one per instance **/
	TArray<FLinearColor> InstanceColorOverride;

	/** Per-variation color override assignments */
	TArray< TArray< FLinearColor > > VariationInstanceColorOverrideArray;
};

//UCLASS()
UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, Activation, "Components|Activation"),
	ShowCategories = (Mobility), editinlinenew)
class UHoudiniAssetComponent_V1 : public UPrimitiveComponent//, public IHoudiniCookHandler
{
	GENERATED_UCLASS_BODY()

public:
	/*
	// IHoudiniCookHandler interface
	virtual FString GetBakingBaseName(const struct FHoudiniGeoPartObject_V1& GeoPartObject) override { return FString(); };
	virtual void SetStaticMeshGenerationParameters(class UStaticMesh* StaticMesh) override {};
	virtual class UMaterialInterface * GetAssignmentMaterial(const FString& MaterialName) override { return nullptr; };
	virtual void ClearAssignmentMaterials() override {};
	virtual void AddAssignmentMaterial(const FString& MaterialName, class UMaterialInterface* MaterialInterface) override {};
	virtual class UMaterialInterface * GetReplacementMaterial(const struct FHoudiniGeoPartObject_V1& GeoPartObject, const FString& MaterialName) override { return nullptr; };
	*/

	/** If true, the physics triangle mesh will use double sided faces when doing scene queries. */
	UPROPERTY(EditAnywhere,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Double Sided Geometry"))
		uint32 bGeneratedDoubleSidedGeometry : 1;

	/** Physical material to use for simple collision on this body. Encodes information about density, friction etc. */
	UPROPERTY(EditAnywhere,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Simple Collision Physical Material"))
		UPhysicalMaterial * GeneratedPhysMaterial;

	/** Default properties of the body instance, copied into objects on instantiation, was URB_BodyInstance */
	UPROPERTY(EditAnywhere, Category = HoudiniGeneratedStaticMeshSettings, meta = (FullyExpand = "true"))
		struct FBodyInstance DefaultBodyInstance;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate. */
	UPROPERTY(EditAnywhere,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Collision Complexity"))
		TEnumAsByte< enum ECollisionTraceFlag > GeneratedCollisionTraceFlag;

	/** Resolution of lightmap. */
	UPROPERTY(EditAnywhere,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Light Map Resolution", FixedIncrement = "4.0"))
		int32 GeneratedLightMapResolution;

	/** Mesh distance field resolution, setting it to 0 will prevent the mesh distance field generation while editing the asset **/
	UPROPERTY(EditAnywhere,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Distance Field Resolution Scale", UIMin = "0.0", UIMax = "100.0"))
		float GeneratedDistanceFieldResolutionScale;

	/** Custom walkable slope setting for generated mesh's body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Walkable Slope Override"))
		FWalkableSlopeOverride GeneratedWalkableSlopeOverride;

	/** The light map coordinate index. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Light map coordinate index"))
		int32 GeneratedLightMapCoordinateIndex;

	/** True if mesh should use a less-conservative method of mip LOD texture factor computation. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Use Maximum Streaming Texel Ratio"))
		uint32 bGeneratedUseMaximumStreamingTexelRatio : 1;

	/** Allows artists to adjust the distance where textures using UV 0 are streamed in/out. */
	UPROPERTY(EditAnywhere, AdvancedDisplay,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Streaming Distance Multiplier"))
		float GeneratedStreamingDistanceMultiplier;

	/** Default settings when using this mesh for instanced foliage. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Foliage Default Settings"))
		UFoliageType_InstancedStaticMesh * GeneratedFoliageDefaultSettings;

	/** Array of user data stored with the asset. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced,
		Category = HoudiniGeneratedStaticMeshSettings,
		meta = (DisplayName = "Asset User Data"))
		TArray<UAssetUserData*> GeneratedAssetUserData;

	/** The output folder for baking actions */
	UPROPERTY()
		FText BakeFolder;

	/** The temporary output folder for cooking actions */
	UPROPERTY()
		FText TempCookFolder;

	virtual void Serialize(FArchive & Ar) override;

	/** Houdini Asset associated with this component. **/
	UHoudiniAsset* HoudiniAsset;

	/** Unique GUID created by component. **/
	FGuid ComponentGUID;

	/** Scale factor used for generated geometry of this component. **/
	float GeneratedGeometryScaleFactor;

	/** Scale factor used for geo transforms of this component. **/
	float TransformScaleFactor;

	/** Buffer to hold preset data for serialization purposes. Used only during serialization. **/
	TArray<char> PresetBuffer;

	/** Buffer to hold default preset for reset purposes. **/
	TArray<char> DefaultPresetBuffer;

	/** Parameters for this component's asset, indexed by parameter id. **/
	//TMap<HAPI_ParmId, UHoudiniAssetParameter_V1 *> Parameters;
	TMap<int, UHoudiniAssetParameter *> Parameters;

	/** Parameters for this component's asset, indexed by name for fast look up. **/
	TMap<FString, UHoudiniAssetParameter *> ParameterByName;

	/** Inputs for this component's asset. **/
	TArray<UHoudiniAssetInput *> Inputs;

	/** Instance inputs for this component's asset **/
	TArray<UHoudiniAssetInstanceInput *> InstanceInputs;

	/** Material assignments. **/
	UHoudiniAssetComponentMaterials_V1 * HoudiniAssetComponentMaterials;

	/** Map of HAPI objects and corresponding static meshes. Also map of static meshes and corresponding components. **/
	TMap<FHoudiniGeoPartObject_V1, UStaticMesh *> StaticMeshes;
	TMap<UStaticMesh *, UStaticMeshComponent *> StaticMeshComponents;

	/** List of dependent downstream asset connections that have this asset as an asset input. **/
	TMap<UHoudiniAssetComponent_V1 *, TSet<int32>> DownstreamAssetConnections;

	/** Map of asset handle components. **/
	TMap<FString, UHoudiniHandleComponent_V1 *> HandleComponents;

	/** Map of curve / spline components. **/
	TMap<FHoudiniGeoPartObject_V1, UHoudiniSplineComponent*> SplineComponents;

	/** Map of Landscape / Heightfield components. **/
	TMap<FHoudiniGeoPartObject_V1, TWeakObjectPtr<ALandscapeProxy>> LandscapeComponents;

	/** Overrides for baking names per part */
	TMap<FHoudiniGeoPartObject_V1, FString> BakeNameOverrides;

	/** Import axis. **/
	uint8 ImportAxis;

	/** Flags used by Houdini component. **/
	union
	{
		struct
		{
			/** Enables cooking for this Houdini Asset. **/
			uint32 bEnableCooking : 1;

			/** Enables uploading of transformation changes back to Houdini Engine. **/
			uint32 bUploadTransformsToHoudiniEngine : 1;

			/** Enables cooking upon transformation changes. **/
			uint32 bTransformChangeTriggersCooks : 1;

			/** Is set to true when this component contains Houdini logo geometry. **/
			uint32 bContainsHoudiniLogoGeometry : 1;

			/** Is set to true when this component is native and false is when it is dynamic. **/
			uint32 bIsNativeComponent : 1;

			/** Is set to true when this component belongs to a preview actor. **/
			uint32 bIsPreviewComponent : 1;

			/** Is set to true if this component has been loaded. **/
			uint32 bLoadedComponent : 1;

			/** Unused **/
			uint32 bIsPlayModeActive_Unused : 1;

			/** unused flag **/
			uint32 bTimeCookInPlaymode_Unused : 1;

			/** Is set to true when Houdini materials are used. **/
			uint32 bUseHoudiniMaterials : 1;

			/** Is set to true when cooking this asset will trigger cooks of downstream connected assets. **/
			uint32 bCookingTriggersDownstreamCooks : 1;

			/** Is set to true after the asset is fully loaded and registered **/
			uint32 bFullyLoaded : 1;
		};

		uint32 HoudiniAssetComponentFlagsPacked;
	};
};

UCLASS()
class UHoudiniInstancedActorComponent_V1 : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	bool UpdateFromLegacyData(UHoudiniInstancedActorComponent* NewIAC);

	//UPROPERTY(SkipSerialization, VisibleAnywhere, Category = Instances)
	UObject* InstancedAsset;

	//UPROPERTY(SkipSerialization, VisibleInstanceOnly, Category = Instances)
	TArray<AActor*> Instances;
};

UCLASS()
class UHoudiniMeshSplitInstancerComponent_V1 : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Serialize(FArchive & Ar) override;

	bool UpdateFromLegacyData(UHoudiniMeshSplitInstancerComponent* NewMSIC);

	//UPROPERTY(SkipSerialization, VisibleInstanceOnly, Category = Instances)
	TArray<UStaticMeshComponent*> Instances;

	//UPROPERTY(SkipSerialization, VisibleInstanceOnly, Category = Instances)
	UMaterialInterface* OverrideMaterial;

	//UPROPERTY(SkipSerialization, VisibleAnywhere, Category = Instances)
	UStaticMesh* InstancedMesh;
};