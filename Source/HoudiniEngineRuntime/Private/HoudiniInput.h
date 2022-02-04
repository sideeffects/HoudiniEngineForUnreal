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

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Optional.h"

#include "HoudiniInputTypes.h"
#include "HoudiniInputObject.h"

#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"

#include "HoudiniInput.generated.h"



class FReply;

enum class EHoudiniCurveType : int8;
enum class ECheckBoxState : unsigned char;

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniInput : public UObject
{
	GENERATED_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:

	UHoudiniInput();

	// Equality operator,
	// We consider two inputs equals if they have the same name, objparam state, and input index/parmId
	// TODO: ParmId might be an incorrect condition
	bool operator==(const UHoudiniInput& other) const
	{
		return (bIsObjectPathParameter == other.bIsObjectPathParameter 
			&& InputIndex == other.InputIndex
			&& ParmId == other.ParmId
			&& Name.Equals(other.Name)
			&& Label.Equals(other.Label));
	}

	bool Matches(const UHoudiniInput& other) const { return (*this == other); };

	// Helper function returning a string from an InputType
	static FString InputTypeToString(const EHoudiniInputType& InInputType);

	// Helper function returning an InputType from a string
	static EHoudiniInputType StringToInputType(const FString& InInputTypeString);
	// Helper function returning a Houdini curve type from a string
	static EHoudiniCurveType StringToHoudiniCurveType(const FString& CurveTypeString);
	// Helper function returning a Houdini curve method from a string
	static EHoudiniCurveMethod StringToHoudiniCurveMethod(const FString& CurveMethodString);
	// Helper function returning a Houdini curve breakpoint parameterization from a string
	static EHoudiniCurveBreakpointParameterization StringToHoudiniCurveBreakpointParameterization(const FString& CurveParameterizationString);
	
	// Helper function indicating what classes are supported by an input type
	static TArray<const UClass*> GetAllowedClasses(const EHoudiniInputType& InInputType);

	// Helper function indicating if an object is supported by an input type	
	static bool IsObjectAcceptable(const EHoudiniInputType& InInputType, const UObject* InObject);

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------

	// Returns the NodeId of the asset / object merge we are associated with
	int32 GetAssetNodeId() const { return AssetNodeId; };
	// For objpath parameter, return the associated ParamId, -1 if we're a Geo input
	int32 GetParameterId() const { return bIsObjectPathParameter ? ParmId : -1; };
	// Returns the NodeId of the node plugged into this input
	int32 GetInputNodeId() const { return InputNodeId; };

	// For Geo inputs, returns the InputIndex, -1 if we're an object path parameter
	int32 GetInputIndex() const { return bIsObjectPathParameter ? -1 : InputIndex; };
	// Return the array containing all the nodes created for this input's data
	TArray<int32>& GetCreatedDataNodeIds() { return CreatedDataNodeIds; };
	// Returns the current input type
	EHoudiniInputType GetInputType() const { return Type; };
	// Returns the previous input type
	EHoudiniInputType GetPreviousInputType() const { return PreviousType; };
	// Returns the current input type as a string
	FString GetInputTypeAsString() const { return InputTypeToString(Type); };

	EHoudiniXformType GetDefaultXTransformType();
	// Returns true when this input's Transform Type is set to NONE,
	// false if set to INTO_THIS_OBJECT, 2 will use the input's type default value
	bool GetKeepWorldTransform() const;
	// Indicates if this input has changed and should be updated
	bool HasChanged();
	// Indicates if this input needs to trigger an update
	bool NeedsToTriggerUpdate();
	// Indicates this input should upload its data
	bool IsDataUploadNeeded();
	// Indicates this input's transform need to be uploaded
	bool IsTransformUploadNeeded();
	// Indicates if this input type has been changed
	bool HasInputTypeChanged() const { return PreviousType != EHoudiniInputType::Invalid ? PreviousType != Type : false; }
	// 
	bool GetUpdateInputLandscape() const;

	void SetUpdateInputLandscape(const bool bInUpdateInputLandcape);

	FString GetName() const					{ return Name; };
	FString GetLabel() const				{ return Label; };
	FString GetHelp() const					{ return Help; };	
	bool GetPackBeforeMerge() const			{ return bPackBeforeMerge; };
	bool GetImportAsReference() const		{ return bImportAsReference; };
	bool GetImportAsReferenceRotScaleEnabled() const		{ return bImportAsReferenceRotScaleEnabled; };
	bool GetExportLODs() const				{ return bExportLODs; };
	bool GetExportSockets() const			{ return bExportSockets; };
	bool GetExportColliders() const			{ return bExportColliders; };
	bool IsObjectPathParameter() const		{ return bIsObjectPathParameter; };
	float GetUnrealSplineResolution() const { return UnrealSplineResolution; };
	
	virtual bool GetCookOnCurveChange() const		{ return bCookOnCurveChanged; };
		
	TArray<UHoudiniInputObject*>* GetHoudiniInputObjectArray(const EHoudiniInputType& InType);
	const TArray<UHoudiniInputObject*>* GetHoudiniInputObjectArray(const EHoudiniInputType& InType) const;
	TArray<AActor*>* GetBoundSelectorObjectArray();
	const TArray<AActor*>* GetBoundSelectorObjectArray() const;

	UHoudiniInputObject* GetHoudiniInputObjectAt(const int32& AtIndex);
	const UHoudiniInputObject* GetHoudiniInputObjectAt(const int32& AtIndex) const;
	AActor* GetBoundSelectorObjectAt(const int32& AtIndex);

	UHoudiniInputObject* GetHoudiniInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex);

	UObject* GetInputObjectAt(const int32& AtIndex);
	UObject* GetInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex);

	int32 GetNumberOfInputObjects();
	int32 GetNumberOfInputObjects(const EHoudiniInputType& InType);

	int32 GetNumberOfInputMeshes();
	int32 GetNumberOfInputMeshes(const EHoudiniInputType& InType);

	int32 GetNumberOfBoundSelectorObjects() const;

	bool IsWorldInputBoundSelector() const { return bIsWorldInputBoundSelector; };
	bool GetWorldInputBoundSelectorAutoUpdates() const { return bWorldInputBoundSelectorAutoUpdate; };

	FString GetNodeBaseName() const;

	bool IsTransformUIExpanded(const int32& AtIndex);

	// Return the transform offset for a given input object
	FTransform* GetTransformOffset(const int32& AtIndex);
	const FTransform GetTransformOffset(const int32& AtIndex) const;

	// Returns the position offset for a given input object
	TOptional<float> GetPositionOffsetX(int32 AtIndex) const;
	TOptional<float> GetPositionOffsetY(int32 AtIndex) const;
	TOptional<float> GetPositionOffsetZ(int32 AtIndex) const;

	// Returns the rotation offset for a given input object
	TOptional<float> GetRotationOffsetRoll(int32 AtIndex) const;
	TOptional<float> GetRotationOffsetPitch(int32 AtIndex) const;
	TOptional<float> GetRotationOffsetYaw(int32 AtIndex) const;

	// Returns the scale offset for a given input object
	TOptional<float> GetScaleOffsetX(int32 AtIndex) const;
	TOptional<float> GetScaleOffsetY(int32 AtIndex) const;
	TOptional<float> GetScaleOffsetZ(int32 AtIndex) const;

	// Returns true if the object is one of our input object for the given type
	bool ContainsInputObject(const UObject* InObject, const EHoudiniInputType& InType) const;

	// Get all input object arrays
	TArray<const TArray<UHoudiniInputObject*>*> GetAllObjectArrays() const;
	TArray<TArray<UHoudiniInputObject*>*> GetAllObjectArrays();

	// Iterate over all input object arrays
	void ForAllHoudiniInputObjectArrays(TFunctionRef<void(const TArray<UHoudiniInputObject*>&)> Fn) const;
	void ForAllHoudiniInputObjectArrays(TFunctionRef<void(TArray<UHoudiniInputObject*>&)> Fn);

	// Return ALL input objects. Optionally, the results can be filtered to only return input objects
	// relevant to the current *input type*.
	void ForAllHoudiniInputObjects(TFunctionRef<void(UHoudiniInputObject*)> Fn, const bool bFilterByInputType=false) const;
	// Collect top-level HoudiniInputObjects from this UHoudiniInput. Does not traverse nested input objects.
	void GetAllHoudiniInputObjects(TArray<UHoudiniInputObject*>& OutObjects) const;
	// Collect top-level UHoudiniInputSceneComponent from this UHoudiniInput. Does not traverse nested input objects.
	void ForAllHoudiniInputSceneComponents(TFunctionRef<void(class UHoudiniInputSceneComponent*)> Fn) const;
	void GetAllHoudiniInputSceneComponents(TArray<class UHoudiniInputSceneComponent*>& OutObjects) const;
	void GetAllHoudiniInputSplineComponents(TArray<class UHoudiniInputHoudiniSplineComponent*>& OutObjects) const;

	// Remove all instances of this input object from all object arrays.
	void RemoveHoudiniInputObject(UHoudiniInputObject* InInputObject);

	bool IsAddRotAndScaleAttributesEnabled() const { return bAddRotAndScaleAttributesOnCurves; };

	bool IsUseLegacyInputCurvesEnabled() const { return bUseLegacyInputCurves; };

	const TSet< ULandscapeComponent * > GetLandscapeSelectedComponents() const { return LandscapeSelectedComponents; };

	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------

	void MarkChanged(const bool& bInChanged)
	{
		bHasChanged = bInChanged;
		SetNeedsToTriggerUpdate(bInChanged);
	};
	void SetNeedsToTriggerUpdate(const bool& bInTriggersUpdate) { bNeedsToTriggerUpdate = bInTriggersUpdate; };
	void MarkDataUploadNeeded(const bool& bInDataUploadNeeded) { bDataUploadNeeded = bInDataUploadNeeded; };
	void MarkAllInputObjectsChanged(const bool& bInChanged);

	void SetSOPInput(const int32& InInputIndex);
	void SetObjectPathParameter(const int32& InParmId);	
	void SetKeepWorldTransform(const bool& bInKeepWorldTransform);

	void SetName(const FString& InName)								{ Name = InName; };
	void SetLabel(const FString& InLabel)							{ Label = InLabel; };
	void SetHelp(const FString& InHelp)								{ Help = InHelp; };
	void SetAssetNodeId(const int32& InNodeId)						{ AssetNodeId = InNodeId; };
	void SetInputType(const EHoudiniInputType &InInputType, bool& bOutBlueprintStructureModified);
	void SetPreviousInputType(const EHoudiniInputType& InType)		{ PreviousType = InType; };
	void SetPackBeforeMerge(const bool& bInPackBeforeMerge)			{ bPackBeforeMerge = bInPackBeforeMerge; };
	void SetImportAsReference(const bool& bInImportAsReference)		{ bImportAsReference = bInImportAsReference; };
	void SetImportAsReferenceRotScaleEnabled(const bool& bInImportAsReferenceRotScaleEnabled)		{ bImportAsReferenceRotScaleEnabled = bInImportAsReferenceRotScaleEnabled; };
	void SetExportLODs(const bool& bInExportLODs)					{ bExportLODs = bInExportLODs; };
	void SetExportSockets(const bool& bInExportSockets)				{ bExportSockets = bInExportSockets; };
	void SetExportColliders(const bool& bInExportColliders)			{ bExportColliders = bInExportColliders; };
	void SetInputNodeId(const int32& InCreatedNodeId)				{ InputNodeId = InCreatedNodeId; };
	void SetUnrealSplineResolution(const float& InResolution)		{ UnrealSplineResolution = InResolution; };

	virtual void SetCookOnCurveChange(const bool & bInCookOnCurveChanged)	{ bCookOnCurveChanged = bInCookOnCurveChanged; };

	void ResetDefaultCurveOffset()								    { DefaultCurveOffset = 0.f; }

	UHoudiniInputObject* CreateNewCurveInputObject(bool& bBlueprintStructureModified);

	void SetGeometryInputObjectsNumber(const int32& NewCount);
	void SetInputObjectsNumber(const EHoudiniInputType& InType, const int32& InNewCount);
	
	void InsertInputObjectAt(const int32& AtIndex);
	void InsertInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex);

	void DeleteInputObjectAt(const int32& AtIndex, const bool bInRemoveIndexFromArray=true);
	void DeleteInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex, const bool bInRemoveIndexFromArray=true);

	void DuplicateInputObjectAt(const int32& AtIndex);
	void DuplicateInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex);

	void SetInputObjectAt(const int32& AtIndex, UObject* InObject);
	void SetInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex, UObject* InObject);

	void SetBoundSelectorObjectsNumber(const int32& InNewCount);
	void SetBoundSelectorObjectAt(const int32& AtIndex, AActor* InActor);
	void SetWorldInputBoundSelector(const bool& InIsBoundSelector) { bIsWorldInputBoundSelector = InIsBoundSelector; };
	void SetWorldInputBoundSelectorAutoUpdates(const bool& InAutoUpdate) { bWorldInputBoundSelectorAutoUpdate = InAutoUpdate; };

	// Updates the world selection using bound selectors
	// returns false if the selection hasn't changed
	bool UpdateWorldSelectionFromBoundSelectors();
	// Updates the world selection
	// returns false if the selection hasn't changed
	bool UpdateWorldSelection(const TArray<AActor*>& InNewSelection);

	void OnTransformUIExpand(const int32& AtIndex);

	// Sets the input's transform offset
	bool SetTransformOffsetAt(const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex);

	// Sets the input's transform scale values
	void SetPositionOffsetX(float InValue, int32 AtIndex);
	void SetPositionOffsetY(float InValue, int32 AtIndex);
	void SetPositionOffsetZ(float InValue, int32 AtIndex);

	// Sets the input's transform rotation value
	void SetRotationOffsetRoll(float InValue, int32 AtIndex);
	void SetRotationOffsetPitch(float InValue, int32 AtIndex);
	void SetRotationOffsetYaw(float InValue, int32 AtIndex);

	// Sets the input's transform scale values
	void SetScaleOffsetX(float InValue, int32 AtIndex);
	void SetScaleOffsetY(float InValue, int32 AtIndex);
	void SetScaleOffsetZ(float InValue, int32 AtIndex);

	void SetAddRotAndScaleAttributes(const bool& InValue);
	void SetUseLegacyInputCurve(const bool& InValue);

	// Duplicate this object and copy its state to the resulting object.
	// This is typically used to transfer state between between template and instance components.
	UHoudiniInput* DuplicateAndCopyState(UObject* DestOuter, bool bInCanDeleteHoudiniNodes);
	virtual void CopyStateFrom(UHoudiniInput*  InInput, bool bCopyAllProperties, bool bInCanDeleteHoudiniNodes);

	void SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes);
	bool CanDeleteHoudiniNodes() { return bCanDeleteHoudiniNodes; }

	virtual void InvalidateData();

protected:
	void CopyInputs(TArray<UHoudiniInputObject*>& ToInputs, TArray<UHoudiniInputObject*>& FromInputs, bool bInCanDeleteHoudiniNodes);

public:

	// Create a Houdini Spline input component, with an existing Houdini Spline input Object.
	// Pass in nullptr to create a default Houdini Spline
	UHoudiniInputHoudiniSplineComponent* CreateHoudiniSplineInput(
		UHoudiniInputHoudiniSplineComponent* FromHoudiniSplineInputObject, 
		const bool & bAttachToParent,
		const bool & bAppendToInputArray,
		bool& bOutBlueprintStructureModified);

	// Given an existing spline input object, remove the associated
	// Houdini Spline component from the owning actor / blueprint.
	void RemoveSplineFromInputObject(
		UHoudiniInputHoudiniSplineComponent* InHoudiniSplineInputObject,
		bool& bOutBlueprintStructureModified) const;

	bool HasLandscapeExportTypeChanged () const;

	void SetHasLandscapeExportTypeChanged(const bool InChanged);

#if WITH_EDITOR
	FText GetCurrentSelectionText() const;
#endif

	EHoudiniLandscapeExportType GetLandscapeExportType() const { return LandscapeExportType; };

	void SetLandscapeExportType(const EHoudiniLandscapeExportType InType) { LandscapeExportType = InType; };

	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	FBox GetBounds() const;

	void UpdateLandscapeInputSelection();

	// Add the current InputNodeId to the pending delete set and set it to -1
	void MarkInputNodeAsPendingDelete();

	// Return the set of previous InputNodeIds that are pending delete
	const TSet<int32>& GetInputNodesPendingDelete() const { return InputNodesPendingDelete; }

	// Clear the InputNodesPendingDelete set 
	void ClearInputNodesPendingDelete() { InputNodesPendingDelete.Empty(); }

#if WITH_EDITORONLY_DATA
	
	UPROPERTY()
	bool bLandscapeUIAdvancedIsExpanded;
	
#endif

protected:

	// Name of the input / Object path parameter
	UPROPERTY()
	FString Name;

	// Label of the SOP input or of the object path parameter
	UPROPERTY()
	FString Label;

	// Input type
	UPROPERTY()
	EHoudiniInputType Type;

	// Previous type, used to detect input type changes
	UPROPERTY(Transient, DuplicateTransient)
	EHoudiniInputType PreviousType;

	// NodeId of the asset / object merge we are associated with
	UPROPERTY(Transient, DuplicateTransient)
	int32 AssetNodeId;

	// NodeId of the created input node 
	// when there is multiple inputs objects, this will be the merge node.
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	int32 InputNodeId;

	// SOP input index (-1 if we're an object path input)
	UPROPERTY()
	int32 InputIndex;

	// Parameter Id of the associated object path parameter (-1 if we're a SOP input)
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	int32 ParmId;

	// Indicates if we're an object path parameter input
	UPROPERTY()
	bool bIsObjectPathParameter;

	// Array containing all the node Ids created by this input
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TArray<int32> CreatedDataNodeIds;

	// Indicates data connected to this input should be uploaded
	UPROPERTY(Transient, DuplicateTransient)
	bool bHasChanged;

	// Indicates this input should trigger an HDA update/cook
	UPROPERTY(Transient, DuplicateTransient)
	bool bNeedsToTriggerUpdate;

	// Indicates data for this input needs to be uploaded
	// If this is false but the input has changed, we may have just updated in input parameter,
	// and don't need to resend all the input data
	bool bDataUploadNeeded;

	// Help for this parameter/input
	UPROPERTY()
	FString Help;

	//-------------------------------------------------------------------------------------------------------------------------
	// General Input options

	// Is set to true when this input's Transform Type is set to NONE, 2 will use the input's default value
	UPROPERTY()
	EHoudiniXformType KeepWorldTransform;

	// Indicates that the geometry must be packed before merging it into the input
	UPROPERTY()
	bool bPackBeforeMerge;

	// Indicates that all the input objects are imported to Houdini as references instead of actual geo
	// (for Geo/World/Asset input types only)
	UPROPERTY()
	bool bImportAsReference = false;

	// Indicates that whether or not to add the rot / scale attributes for reference improts
	UPROPERTY()
	bool bImportAsReferenceRotScaleEnabled = false;
	
	// Indicates that all LODs in the input should be marshalled to Houdini
	UPROPERTY()
	bool bExportLODs;

	// Indicates that all sockets in the input should be marshalled to Houdini
	UPROPERTY()
	bool bExportSockets;

	// Indicates that all colliders in the input should be marshalled to Houdini
	UPROPERTY()
	bool bExportColliders;

	// Indicates that if trigger cook automatically on curve Input spline modified
	UPROPERTY()
	bool bCookOnCurveChanged;

	//-------------------------------------------------------------------------------------------------------------------------
	// Geometry objects
	UPROPERTY()
	TArray<UHoudiniInputObject*> GeometryInputObjects;

	// Is set to true when static mesh used for geometry input has changed.
	UPROPERTY()
	bool bStaticMeshChanged;

#if WITH_EDITORONLY_DATA
	// Are the transform UI expanded ?
	// Values default to false and are actually added to the array in OnTransformUIExpand()
	UPROPERTY()
	TArray<bool> TransformUIExpanded;
#endif

	//-------------------------------------------------------------------------------------------------------------------------
	// Asset inputs
	UPROPERTY()
	TArray<UHoudiniInputObject*> AssetInputObjects;

	// Is set to true if the asset input is actually connected inside Houdini.
	UPROPERTY()
	bool bInputAssetConnectedInHoudini;

	//-------------------------------------------------------------------------------------------------------------------------
	// Curve/Spline inputs
	UPROPERTY()
	TArray<UHoudiniInputObject*> CurveInputObjects;

	// Offset used when using muiltiple curves
	UPROPERTY()
	float DefaultCurveOffset;

	// Set this to true to add rot and scale attributes on curve inputs.
	UPROPERTY()
	bool bAddRotAndScaleAttributesOnCurves;

	// Set this to true to use legacy (curve::1.0) input curves
	UPROPERTY()
	bool bUseLegacyInputCurves;
	
	//-------------------------------------------------------------------------------------------------------------------------
	// Landscape inputs
	UPROPERTY()
	TArray<UHoudiniInputObject*> LandscapeInputObjects;

	UPROPERTY()
	bool bLandscapeHasExportTypeChanged = false;

	//-------------------------------------------------------------------------------------------------------------------------
	// World inputs
	UPROPERTY()
	TArray<UHoudiniInputObject*> WorldInputObjects;

	// Objects used for automatic bound selection
	UPROPERTY()
	TArray<AActor*> WorldInputBoundSelectorObjects;

	// Indicates that this world input is in "BoundSelector" mode
	UPROPERTY()
	bool bIsWorldInputBoundSelector;

	// Indicates that selected actors by the bound selectors should update automatically
	UPROPERTY()
	bool bWorldInputBoundSelectorAutoUpdate;

	// Resolution used when converting unreal splines to houdini curves
	UPROPERTY()
	float UnrealSplineResolution;

	//-------------------------------------------------------------------------------------------------------------------------
	// Skeletal Inputs
	UPROPERTY()
	TArray<UHoudiniInputObject*> SkeletalInputObjects;

	// GeometryCollection inputs
	UPROPERTY()
	TArray<UHoudiniInputObject*> GeometryCollectionInputObjects;

	// A cache of the selected landscape components so that it is saved across levels
	UPROPERTY()
	TSet< ULandscapeComponent * > LandscapeSelectedComponents;

	// The node ids of InputNodeIds previously used by this input that are pending delete
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TSet<int32> InputNodesPendingDelete;

public:

	// This array is to record the last insert action, for undo input insertion actions.
	UPROPERTY(Transient, DuplicateTransient)
	TArray<UHoudiniInputHoudiniSplineComponent*> LastInsertedInputs;

	// This array is to cache the action of last undo delete action, and redo that action.
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TArray<UHoudiniInputObject*> LastUndoDeletedInputs;


	// Indicates that the landscape input's source landscape should be updated instead of creating a new component
	UPROPERTY()
	bool bUpdateInputLandscape;

	// Indicates if the landscape should be exported as heightfield, mesh or points
	UPROPERTY()
	EHoudiniLandscapeExportType LandscapeExportType = EHoudiniLandscapeExportType::Heightfield;

	// Is set to true when landscape input is set to selection only.
	UPROPERTY()
	bool bLandscapeExportSelectionOnly = false;

	// Is set to true when the automatic selection of landscape component is active
	UPROPERTY()
	bool bLandscapeAutoSelectComponent = false;

	// Is set to true when materials are to be exported.
	UPROPERTY()
	bool bLandscapeExportMaterials = false;

	// Is set to true when lightmap information export is desired.
	UPROPERTY()
	bool bLandscapeExportLighting = false;

	// Is set to true when uvs should be exported in [0,1] space.
	UPROPERTY()
	bool bLandscapeExportNormalizedUVs = false;

	// Is set to true when uvs should be exported for each tile separately.
	UPROPERTY()
	bool bLandscapeExportTileUVs = false;

	UPROPERTY()
	bool bCanDeleteHoudiniNodes = true;
};