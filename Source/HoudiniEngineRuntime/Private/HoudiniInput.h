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

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	
	virtual void PostLoad() override;

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

	FString GetName() const					{ return Name; };
	FString GetLabel() const				{ return Label; };
	FString GetHelp() const					{ return Help; };	
	bool GetPackBeforeMerge() const				{ return bPackBeforeMerge; };
	bool GetImportAsReference() const			{ return InputSettings.bImportAsReference; };
	bool GetImportAsReferenceRotScaleEnabled() const	{ return InputSettings.bImportAsReferenceRotScaleEnabled; };
	bool GetImportAsReferenceBboxEnabled() const		{ return InputSettings.bImportAsReferenceBboxEnabled; };
	bool GetImportAsReferenceMaterialEnabled() const	{ return InputSettings.bImportAsReferenceMaterialEnabled; };
	bool GetExportLODs() const				{ return InputSettings.bExportLODs; };
	bool GetExportSockets() const				{ return InputSettings.bExportSockets; };
	bool GetPreferNaniteFallbackMesh() const		{ return InputSettings.bPreferNaniteFallbackMesh; }
	bool GetExportColliders() const				{ return InputSettings.bExportColliders; };
	bool GetExportMaterialParameters() const		{ return InputSettings.bExportMaterialParameters; };
	bool IsObjectPathParameter() const			{ return bIsObjectPathParameter; };
	float GetUnrealSplineResolution() const			{ return InputSettings.UnrealSplineResolution; };
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

	bool IsAddRotAndScaleAttributesEnabled() const { return InputSettings.bAddRotAndScaleAttributesOnCurves; };

	bool IsUseLegacyInputCurvesEnabled() const { return InputSettings.bUseLegacyInputCurves; };

	bool IsLandscapeAutoSelectComponentEnabled() const { return InputSettings.bLandscapeAutoSelectComponent; }
	bool IsLandscapeExportSelectionOnlyEnabled() const { return InputSettings.bLandscapeExportSelectionOnly; }
	bool IsLandscapeExportLightingEnabled() const { return InputSettings.bLandscapeExportLighting; }
	bool IsLandscapeExportMaterialsEnabled() const { return InputSettings.bLandscapeExportMaterials; }
	bool IsLandscapeExportNormalizedUVsEnabled() const { return InputSettings.bLandscapeExportNormalizedUVs; }
	bool IsLandscapeExportTileUVsEnabled() const { return InputSettings.bLandscapeExportTileUVs; }

	bool IsPerLayerExportEnabled() const { return InputSettings.bExportPerEditLayerData; }

	const TSet< ULandscapeComponent * > GetLandscapeSelectedComponents() const { return LandscapeSelectedComponents; };

	// Get a constant reference to the InputSettings
	const FHoudiniInputObjectSettings& GetInputSettings() const { return InputSettings; }

	// Copy the InputSettings into OutInputSettings.
	void CopyInputSettingsTo(FHoudiniInputObjectSettings& OutInputSettings) const { OutInputSettings = InputSettings; }

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
	void SetPreviousInputType(const EHoudiniInputType& InType)					{ PreviousType = InType; };
	void SetPackBeforeMerge(const bool& bInPackBeforeMerge)						{ bPackBeforeMerge = bInPackBeforeMerge; };
	void SetImportAsReference(const bool& bInImportAsReference)					{ InputSettings.bImportAsReference = bInImportAsReference; };
	void SetImportAsReferenceRotScaleEnabled(const bool& bInImportAsReferenceRotScaleEnabled)	{ InputSettings.bImportAsReferenceRotScaleEnabled = bInImportAsReferenceRotScaleEnabled; };
	void SetImportAsReferenceBboxEnabled(const bool& bInImportAsReferenceBboxEnabled)		{ InputSettings.bImportAsReferenceBboxEnabled = bInImportAsReferenceBboxEnabled; };
	void SetImportAsReferenceMaterialEnabled(const bool& bInImportAsReferenceMaterialEnabled)	{ InputSettings.bImportAsReferenceMaterialEnabled = bInImportAsReferenceMaterialEnabled; };
	void SetExportLODs(const bool& bInExportLODs)							{ InputSettings.bExportLODs = bInExportLODs; };
	void SetExportSockets(const bool& bInExportSockets)						{ InputSettings.bExportSockets = bInExportSockets; };
	void SetPreferNaniteFallbackMesh(const bool& bInPreferNaniteFallbackMesh)			{ InputSettings.bPreferNaniteFallbackMesh = bInPreferNaniteFallbackMesh; };
	void SetExportColliders(const bool& bInExportColliders)						{ InputSettings.bExportColliders = bInExportColliders; };
	void SetExportMaterialParameters(const bool& bInExportMaterialParameters)			{ InputSettings.bExportMaterialParameters = bInExportMaterialParameters; };
	void SetInputNodeId(const int32& InCreatedNodeId)						{ InputNodeId = InCreatedNodeId; };
	void SetUnrealSplineResolution(const float& InResolution)					{ InputSettings.UnrealSplineResolution = InResolution; };
	void SetPerLayerExportEnabled(bool bOnOff) { InputSettings.bExportPerEditLayerData = bOnOff; }

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

	void SetLandscapeAutoSelectComponentEnabled(bool bInEnabled) { InputSettings.bLandscapeAutoSelectComponent = bInEnabled; }
	void SetLandscapeExportSelectionOnlyEnabled(bool bInEnabled) { InputSettings.bLandscapeExportSelectionOnly = bInEnabled; }
	void SetLandscapeExportLightingEnabled(bool bInEnabled) { InputSettings.bLandscapeExportLighting = bInEnabled; }
	void SetLandscapeExportMaterialsEnabled(bool bInEnabled) { InputSettings.bLandscapeExportMaterials = bInEnabled; }
	void SetLandscapeExportNormalizedUVsEnabled(bool bInEnabled) { InputSettings.bLandscapeExportNormalizedUVs = bInEnabled; }
	void SetLandscapeExportTileUVsEnabled(bool bInEnabled) { InputSettings.bLandscapeExportTileUVs = bInEnabled; }

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

	EHoudiniLandscapeExportType GetLandscapeExportType() const { return InputSettings.LandscapeExportType; };

	void SetLandscapeExportType(const EHoudiniLandscapeExportType InType) { InputSettings.LandscapeExportType = InType; };

	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	FBox GetBounds(UWorld * World);

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

	// Cached Bounds of this input
	// Used when we cannot access the input objects (ie, during GC)
	UPROPERTY()
	FBox CachedBounds;

	// Help for this parameter/input
	UPROPERTY()
	FString Help;

	//-------------------------------------------------------------------------------------------------------------------------
	// General Input options

	// Is set to true when this input's Transform Type is set to NONE, 2 will use the input's default value
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	EHoudiniXformType KeepWorldTransform;

	// Indicates that the geometry must be packed before merging it into the input
	UPROPERTY()
	bool bPackBeforeMerge;

	// Indicates that all the input objects are imported to Houdini as references instead of actual geo
	// (for Geo/World/Asset input types only)
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bImportAsReference = false;

	// Indicates that whether or not to add the rot / scale attributes for reference imports
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bImportAsReferenceRotScaleEnabled = true;

	// Indicates whether or not to add bbox attributes for reference imports
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bImportAsReferenceBboxEnabled = true;

	// Indicates whether or not to add material attributes for reference imports
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bImportAsReferenceMaterialEnabled = true;
	
	// Indicates that all LODs in the input should be marshalled to Houdini
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bExportLODs;

	// Indicates that all sockets in the input should be marshalled to Houdini
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bExportSockets;

	// Override property for preferring the Nanite fallback mesh when using a Nanite geometry as input
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bPreferNaniteFallbackMesh;

	// Indicates that all colliders in the input should be marshalled to Houdini
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bExportColliders;

	// Indicates that material parameters should be exported as attributes
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bExportMaterialParameters;

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
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bAddRotAndScaleAttributesOnCurves;

	// Set this to true to use legacy (curve::1.0) input curves
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
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
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
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

	// Enable/disable auto selecting landscape splines (for landscapes via world input).
	void SetLandscapeAutoSelectSplines(const bool bInLandscapeAutoSelectSplines);

	// Is landscape spline auto-selection (for landscapes via world inputs) enabled?
	bool IsLandscapeAutoSelectSplinesEnabled() const { return InputSettings.bLandscapeAutoSelectSplines; }

	// Enable/disable exporting a separate control point cloud for landscape splines
	void SetLandscapeSplinesExportControlPoints(const bool bInLandscapeSendControlPoints) { InputSettings.bLandscapeSplinesExportControlPoints = bInLandscapeSendControlPoints; }

	// Is exporting a separate control point cloud for landscape splines enabled?
	bool IsLandscapeSplinesExportControlPointsEnabled() const { return InputSettings.bLandscapeSplinesExportControlPoints; }

	// Enable/disable exporting left/right curves for landscape splines
	void SetLandscapeSplinesExportLeftRightCurves(const bool bInLandscapeSplinesExportLeftRightCurves) { InputSettings.bLandscapeSplinesExportLeftRightCurves = bInLandscapeSplinesExportLeftRightCurves; }

	// Is exporting left/right curves for landscape splines enabled?
	bool IsLandscapeSplinesExportLeftRightCurvesEnabled() const { return InputSettings.bLandscapeSplinesExportLeftRightCurves; }

	// Enable/disable exporting the spline mesh components for landscape splines
	void SetLandscapeSplinesExportSplineMeshComponents(const bool bInLandscapeSplinesExportSplineMeshes) { InputSettings.bLandscapeSplinesExportSplineMeshComponents = bInLandscapeSplinesExportSplineMeshes; }

	// Is exporting spline mesh components for landscape splines enabled?
	bool IsLandscapeSplinesExportSplineMeshComponentsEnabled() const { return InputSettings.bLandscapeSplinesExportSplineMeshComponents; }

	// Returns true if the landscape splines export menu is expanded.
	bool IsLandscapeSplinesExportOptionsMenuExpanded() const { return bLandscapeSplinesExportOptionsMenuExpanded; }

	// Setter for if the landscape splines export menu is expanded.
	void SetLandscapeSplinesExportOptionsMenuExpanded(const bool bInLandscapeSplinesExportOptionsMenuExpanded) { bLandscapeSplinesExportOptionsMenuExpanded = bInLandscapeSplinesExportOptionsMenuExpanded; }

	// Setter for if spline mesh components should be merged into one SM when exported 
	void SetMergeSplineMeshComponents(const bool bInMergeSplineMeshComponents) { InputSettings.bMergeSplineMeshComponents = bInMergeSplineMeshComponents; }

	// Getter for if spline mesh components should be merged into one SM when exported
	bool IsMergeSplineMeshComponentsEnabled() const { return InputSettings.bMergeSplineMeshComponents; }

	// Remove all landscape splines of the landscape input objects currently in the world input object array.
	// Return true if any objects were removed.
	bool RemoveAllLandscapeSplineActorsForInputLandscapes();

	// Add all landscape splines of the landscape input objects currently in the world input object array as
	// world input objects. Will not add splines that are already in the array.
	// Returns true if any splines were added.
	bool AddAllLandscapeSplineActorsForInputLandscapes();
	
	// This array is to record the last insert action, for undo input insertion actions.
	UPROPERTY(Transient, DuplicateTransient)
	TArray<UHoudiniInputHoudiniSplineComponent*> LastInsertedInputs;

	// This array is to cache the action of last undo delete action, and redo that action.
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TArray<UHoudiniInputObject*> LastUndoDeletedInputs;


	// Indicates that the landscape input's source landscape should be updated instead of creating a new component
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use Edit Layers instead."))
	bool bUpdateInputLandscape_DEPRECATED;

	// Indicates if the landscape should be exported as heightfield, mesh or points
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	EHoudiniLandscapeExportType LandscapeExportType = EHoudiniLandscapeExportType::Heightfield;

	// Is set to true when landscape input is set to selection only.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeExportSelectionOnly = false;

	// Is set to true when layer visibility is controlled by the plugin.
	UPROPERTY()
	bool bLandscapeControlVisiblity = false;

	// Is set to true when the automatic selection of landscape component is active
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeAutoSelectComponent = false;

	// Is set to true when materials are to be exported.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeExportMaterials = false;

	// Is set to true when lightmap information export is desired.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeExportLighting = false;

	// Is set to true when uvs should be exported in [0,1] space.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeExportNormalizedUVs = false;

	// Is set to true when uvs should be exported for each tile separately.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeExportTileUVs = false;

	UPROPERTY()
	bool bCanDeleteHoudiniNodes = true;

protected:

	// If true, also export a landscape's splines
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeAutoSelectSplines = false;

	// If true, then the landscape spline export options menu is expanded
	UPROPERTY()
	bool bLandscapeSplinesExportOptionsMenuExpanded = false;
	
	// If true, send a separate control point cloud of the landscape splines control points.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeSplinesExportControlPoints = false;

	// If true, export left and right curves as well
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeSplinesExportLeftRightCurves = false;

	// If true, export the spline mesh components of landscape splines
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bLandscapeSplinesExportSplineMeshComponents = false;

	// If true, the deformed meshes of all spline mesh components of an actor are merged into temporary input mesh.
	// If false, the meshes are sent individually.
	UE_DEPRECATED("2.0.20", "Use the InputSettings struct/accessors instead.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use the InputSettings struct/accessors instead."))
	bool bMergeSplineMeshComponents = true;

	// Various input settings, such as bExportLODs, bExportSockets etc.
	UPROPERTY()
	FHoudiniInputObjectSettings InputSettings;

};