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

#include "HoudiniEngineRuntimeCommon.h"
#include "HoudiniPublicAPIObjectBase.h"

#include "HoudiniPublicAPIInputTypes.generated.h"

class UHoudiniInput;
class UHoudiniInputObject;
class UHoudiniSplineComponent;

/**
 * This class is the base class of a hierarchy that represents an input to an HDA in the public API.
 *
 * Each type of input has a derived class:
 *   - Geometry: UHoudiniPublicAPIGeoInput
 *   - Asset: UHoudiniPublicAPIAssetInput
 *   - Curve: UHoudiniPublicAPICurveInput
 *   - World: UHoudiniPublicAPIWorldInput
 *   - Landscape: UHoudiniPublicAPILandscapeInput
 *
 * Each instance of one of these classes represents the configuration of an input and wraps the actor/object/asset
 * used as the input. These instances are always treated as copies of the actual state of the HDA's input: changing
 * a property of one of these instances does not immediately affect the instantiated HDA: one has to pass the input
 * instances as arguments to UHoudiniPublicAPIAssetWrapper::SetInputAtIndex() or
 * UHoudiniPublicAPIAssetWrapper::SetInputParameter() functions to actually change the inputs on the instantiated asset.
 * A copy of the existing input state of an instantiated HDA can be fetched via
 * UHoudiniPublicAPIAssetWrapper::GetInputAtIndex() and UHoudiniPublicAPIAssetWrapper::GetInputParameter().
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIInput : public UHoudiniPublicAPIObjectBase
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPIInput();

	/** Is set to true when this input's Transform Type is set to NONE, 2 will use the input's default value .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bKeepWorldTransform;

	/**
	 * Indicates that all the input objects are imported to Houdini as references instead of actual geo
	 * (for Geo/World/Asset input types only)
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bImportAsReference;

	/** Returns true if InObject is acceptable for this input type.
	 * @param InObject The object to check for acceptance as an input object on this input.
	 * @return true if InObject is acceptable for this input type.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool IsAcceptableObjectForInput(UObject* InObject) const;
	virtual bool IsAcceptableObjectForInput_Implementation(UObject* InObject) const;

	/**
	 * Sets the specified objects as the input objects.
	 * @param InObjects The objects to set as input objects for this input.
	 * @return false if any object was incompatible (all compatible objects are added).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool SetInputObjects(const TArray<UObject*>& InObjects);

	/**
	 * Gets the currently assigned input objects.
	 * @param OutObjects The current input objects of this input.
	 * @return true if input objects were successfully added to OutObjects (even if there are no input objects).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool GetInputObjects(TArray<UObject*>& OutObjects);

	/**
	 * Populate this input instance from the internal UHoudiniInput instance InInput.
	 * This copies the configuration and UHoudiniInputObject(s).
	 * @param InInput The internal UHoudiniInput to copy to this instance.
	 * @return false if there were any errors while copying the input data.
	 */
	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput);

	/**
	 * Update an internal UHoudiniInput instance InInput from this API input instance.
	 * This copies the configuration and UHoudiniInputObject(s) from this API instance instance to InInput.
	 * @param InInput The internal UHoudiniInput to update from to this API instance.
	 * @return false if there were any errors while copying the input data.
	 */
	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const;

protected:
	/**
	 * Copy any properties we need from the UHoudiniInputObject InSrc for the the input object it wraps.
	 * @param InInputObject The UHoudiniInputObject to copy from.
	 * @param InObject The input object to copy per-object properties for.
	 * @return false if the copy failed, for example if InSrc is invalid or of incompatible type.
	 * @deprecated Use CopyHoudiniInputObjectPropertiesToInputObject instead.
	 */
	UE_DEPRECATED("1.1.0", "Use CopyHoudiniInputObjectPropertiesToInputObject instead.")
	virtual bool CopyHoudiniInputObjectProperties(UHoudiniInputObject const* const InInputObject, UObject* const InObject);

	/**
	 * Copy any properties we need from the UHoudiniInputObject InInputObject for the the input object it wraps.
	 * @param InHoudiniInputObject The UHoudiniInputObject to copy from.
	 * @param InInputObjectIndex The input object to copy per-object properties for.
	 * @return false if the copy failed, for example if InSrc is invalid or of incompatible type.
	 */
	virtual bool CopyHoudiniInputObjectPropertiesToInputObject(UHoudiniInputObject const* const InHoudiniInputObject, const int32 InInputObjectIndex);

	/**
	 * Copy any properties for InObject from this input wrapper to InInputObject.
	 * @param InObject The input object to copy per-object properties for.
	 * @param InInputObject The Houdini input object to copy the properties to.
	 * @return false if the copy failed, for example if InSrc is invalid or of incompatible type.
	 * @deprecated Use CopyInputObjectPropertiesToHoudiniInputObject instead.
	 */
	UE_DEPRECATED("1.1.0", "Use CopyInputObjectPropertiesToHoudiniInputObject instead.")
	virtual bool CopyPropertiesToHoudiniInputObject(UObject* const InObject, UHoudiniInputObject* const InInputObject) const;

	/**
	 * Copy any properties for InInputObjectIndex from this input wrapper to InHoudiniInputObject.
	 * @param InInputObjectIndex The index of the input object to copy per-object properties for.
	 * @param InHoudiniInputObject The Houdini input object to copy the properties to.
	 * @return false if the copy failed, for example if InSrc is invalid or of incompatible type.
	 */
	virtual bool CopyInputObjectPropertiesToHoudiniInputObject(const int32 InInputObjectIndex, UHoudiniInputObject* const InHoudiniInputObject) const;

	/**
	 * Convert an object used as an input in an internal UHoudiniInput to be API compatible.
	 * @param InInternalInputObject An object as an input by UHoudiniInput
	 * @return An API compatible input object created from InInternalInputObject. By default this is just
	 * InInternalInputObject.
	 */
	virtual UObject* ConvertInternalInputObject(UObject* InInternalInputObject) { return InInternalInputObject; }

	/**
	 * Convert an object used as an input in the API to be UHoudiniInput to be compatible and assign it to the
	 * InHoudiniInput at index InIndex.
	 * @param InAPIInputObject An input object in the API.
	 * @param InHoudiniInput The UHoudiniInput to assign the input to.
	 * @param InInputIndex The object index in InHoudiniInput to assign the converted input object to.
	 * @return A UHoudiniInput compatible input object created from InAPIInputObject. By default this is just
	 * InAPIInputObject.
	 */
	virtual UObject* ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const;
	
	/**
	 * Returns the type of the input. Subclasses should override this and return the appropriate input type.
	 * The base class / default implementation returns EHoudiniInputType::Invalid
	 */
	virtual EHoudiniInputType GetInputType() const { return EHoudiniInputType::Invalid; }

	/** The input objects for this input. */
	UPROPERTY()
	TArray<UObject*> InputObjects;

};


/**
 * API wrapper input class for geometry inputs. Derived from UHoudiniPublicAPIInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIGeoInput : public UHoudiniPublicAPIInput
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPIGeoInput();

	/** Indicates that the geometry must be packed before merging it into the input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bPackBeforeMerge;

	/** Indicates that all LODs in the input should be marshalled to Houdini */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bExportLODs;

	/** Indicates that all sockets in the input should be marshalled to Houdini */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bExportSockets;

	/** Indicates that all colliders in the input should be marshalled to Houdini */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bExportColliders;

	/**
	 * Set the transform offset of the specified input object InObject (must already have been set via SetInputObjects()).
	 * @param InObject The input object to set a transform offset for.
	 * @param InTransform The transform offset to set.
	 * @return true if the object was found and the transform offset set.
	 */
	UE_DEPRECATED("1.1.0", "Use SetInputObjectTransformOffset instead.")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs", Meta=(AutoCreateRefTerm="InTransform", DeprecatedFunction, DeprecationMessage="Use SetInputObjectTransformOffset instead."))
	bool SetObjectTransformOffset(UObject* InObject, const FTransform& InTransform);

	/**
	 * Get the transform offset of the specified input object InObject (must already have been set via SetInputObjects()).
	 * @param InObject The input object to get a transform offset for.
	 * @param OutTransform The transform offset that was fetched.
	 * @return true if the object was found and the transform offset fetched.
	 */
	UE_DEPRECATED("1.1.0", "Use GetInputObjectTransformOffset instead.")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs", Meta=(DeprecatedFunction, DeprecationMessage="Use GetInputObjectTransformOffset instead."))
	bool GetObjectTransformOffset(UObject* InObject, FTransform& OutTransform) const;

	/**
	 * Set the transform offset of the input object at the specified index in InputObjects (must already have been set via SetInputObjects()).
	 * @param InInputObjectIndex The input object index to set a transform offset for.
	 * @param InTransform The transform offset to set.
	 * @return true if the index is valid and the transform offset set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs", Meta=(AutoCreateRefTerm="InTransform"))
	bool SetInputObjectTransformOffset(const int32 InInputObjectIndex, const FTransform& InTransform);

	/**
	 * Get the transform offset of the input object at the specified index in InputObjects (must already have been set via SetInputObjects()).
	 * @param InInputObjectIndex The input object index to get a transform offset for.
	 * @param OutTransform The transform offset that was fetched.
	 * @return true if the index is valid and the was transform offset fetched.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool GetInputObjectTransformOffset(const int32 InInputObjectIndex, FTransform& OutTransform) const;

	/**
	 * Get the array of input object transforms.
	 * @param OutInputObjectTransformOffsetArray The output array.
	 * @return false if the input type does not support input object transform offsets. See SupportsTransformOffset.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool GetInputObjectTransformOffsetArray(TArray<FTransform>& OutInputObjectTransformOffsetArray) const;

	/**
	 * Returns true if this input type supports Transform offsets. If false,
	 * functions that get/set transform offsets will return false and set an error:
	 *   - SetObjectTransformOffset
	 *   - GetObjectTransformOffset
	 *   - SetInputObjectTransformOffset
	 *   - GetInputObjectTransformOffset
	 */
	virtual bool SupportsTransformOffset() const { return true; }

	virtual bool SetInputObjects_Implementation(const TArray<UObject*>& InObjects) override;

	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput) override;

	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const override;

	virtual void PostLoad() override;

protected:
	virtual bool CopyHoudiniInputObjectPropertiesToInputObject(UHoudiniInputObject const* const InHoudiniInputObject, const int32 InInputObjectIndex) override;

	virtual bool CopyInputObjectPropertiesToHoudiniInputObject(const int32 InInputObjectIndex, UHoudiniInputObject* const InHoudiniInputObject) const override;

	virtual EHoudiniInputType GetInputType() const override { return EHoudiniInputType::Geometry; }

	/** Per-Input-Object data: the transform offset per input object. */
	UE_DEPRECATED("1.1.0", "Use InputObjectTransformOffsetArray via SetInputObjectTransformOffset and GetInputObjectTransformOffset instead")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use InputObjectTransformOffsetArray instead"))
	TMap<UObject*, FTransform> InputObjectTransformOffsets_DEPRECATED;

private:
	/** Transform offset per input object (by input object array index). */
	UPROPERTY()
	TArray<FTransform> InputObjectTransformOffsetArray;
};

UENUM(BlueprintType)
enum class EHoudiniPublicAPICurveType : uint8
{
	Invalid = 0,

	Polygon = 1,
	Nurbs   = 2,
	Bezier  = 3,
	Points  = 4
};

UENUM(BlueprintType)
enum class EHoudiniPublicAPICurveMethod : uint8
{
	Invalid = 0,

	CVs = 1,
	Breakpoints = 2,
	Freehand = 3
};

UENUM(BlueprintType)
enum class EHoudiniPublicAPICurveBreakpointParameterization : uint8
{
	Invalid = 0,

	Uniform = 1,
	Chord = 2,
	Centripetal = 3
};

/**
 * API wrapper input class for curve inputs. Derived from UHoudiniPublicAPIInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs | Input Objects")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPICurveInputObject : public UHoudiniPublicAPIObjectBase
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPICurveInputObject();

	/**
	 * Set the points of the curve (replacing any previously set points with InCurvePoints).
	 * @param InCurvePoints The new points to set / replace the curve's points with.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void SetCurvePoints(const TArray<FTransform>& InCurvePoints);
	FORCEINLINE
	virtual void SetCurvePoints_Implementation(const TArray<FTransform>& InCurvePoints) { CurvePoints = InCurvePoints; }

	/**
	 * Append a point to the end of this curve.
	 * @param InCurvePoint The point to append.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Meta=(AutoCreateRefTerm="InCurvePoint"), Category="Houdini Engine | Public API | Inputs | Input Objects")
	void AppendCurvePoint(const FTransform& InCurvePoint);
	FORCEINLINE
	virtual void AppendCurvePoint_Implementation(const FTransform& InCurvePoint) { CurvePoints.Add(InCurvePoint); }

	/** Remove all points from the curve. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void ClearCurvePoints();
	FORCEINLINE
	virtual void ClearCurvePoints_Implementation() { CurvePoints.Empty(); }

	/**
	 * Get all points of the curve.
	 * @param OutCurvePoints Set to a copy of all of the points of the curve.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void GetCurvePoints(TArray<FTransform>& OutCurvePoints) const;
	FORCEINLINE
	virtual void GetCurvePoints_Implementation(TArray<FTransform>& OutCurvePoints) const { OutCurvePoints = CurvePoints; }

	/** Returns true if this is a closed curve. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	bool IsClosed() const;
	FORCEINLINE
	virtual bool IsClosed_Implementation() const { return bClosed; }

	/**
	 * Set whether the curve is closed or not.
	 * @param bInClosed The new closed setting for the curve: set to true if the curve is closed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void SetClosed(const bool bInClosed);
	FORCEINLINE
	virtual void SetClosed_Implementation(const bool bInClosed) { bClosed = bInClosed; }

	/** Returns true if the curve is reversed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	bool IsReversed() const;
	FORCEINLINE
	virtual bool IsReversed_Implementation() const { return bReversed; }

	/**
	 * Set whether the curve is reversed or not.
	 * @param bInReversed The new reversed setting for the curve: set to true if the curve is reversed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void SetReversed(const bool bInReversed);
	FORCEINLINE
	virtual void SetReversed_Implementation(const bool bInReversed) { bReversed = bInReversed; }

	/** Returns the curve type (for example: polygon, nurbs, bezier) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	EHoudiniPublicAPICurveType GetCurveType() const;
	FORCEINLINE
	virtual EHoudiniPublicAPICurveType GetCurveType_Implementation() const { return CurveType; }

	/**
	 * Set the curve type (for example: polygon, nurbs, bezier).
	 * @param InCurveType The new curve type.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void SetCurveType(const EHoudiniPublicAPICurveType InCurveType);
	FORCEINLINE
	virtual void SetCurveType_Implementation(const EHoudiniPublicAPICurveType InCurveType) { CurveType = InCurveType; }

	/** Get the curve method, for example CVs, or freehand. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	EHoudiniPublicAPICurveMethod GetCurveMethod() const;
	FORCEINLINE
	virtual EHoudiniPublicAPICurveMethod GetCurveMethod_Implementation() const { return CurveMethod; }

	/**
	 * Set the curve method, for example CVs, or freehand.
	 * @param InCurveMethod The new curve method.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void SetCurveMethod(const EHoudiniPublicAPICurveMethod InCurveMethod);
	FORCEINLINE
	virtual void SetCurveMethod_Implementation(const EHoudiniPublicAPICurveMethod InCurveMethod) { CurveMethod = InCurveMethod; }

	/** Get the curve breakpoint parameterization, for example Uniform, or Chord. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	EHoudiniPublicAPICurveBreakpointParameterization GetCurveBreakpointParameterization() const;
	FORCEINLINE
	virtual EHoudiniPublicAPICurveBreakpointParameterization GetCurveBreakpointParameterization_Implementation() const { return CurveBreakpointParameterization; }

	/**
	* Set the curve breakpoint parameterization, for example Uniform, or Chord.
	* @param InCurveBreakpointParameterization The new curve method.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs | Input Objects")
	void SetCurveBreakpointParameterization(const EHoudiniPublicAPICurveBreakpointParameterization InCurveBreakpointParameterization);
	FORCEINLINE
	virtual void SetCurveBreakpointParameterization_Implementation(const EHoudiniPublicAPICurveBreakpointParameterization InCurveBreakpointParameterization) { CurveBreakpointParameterization = InCurveBreakpointParameterization; }

	/**
	 * Populate this wrapper from a UHoudiniSplineComponent.
	 * @param InSpline The spline to populate this wrapper from.
	 */
	void PopulateFromHoudiniSplineComponent(UHoudiniSplineComponent const* const InSpline);

	/**
	 * Copies the curve data to a UHoudiniSplineComponent.
	 * @param InSpline The spline to copy to.
	 */
	void CopyToHoudiniSplineComponent(UHoudiniSplineComponent* const InSpline) const;

protected:
	/** The control points of the curve. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Houdini Engine | Public API | Inputs | Input Objects")
	TArray<FTransform> CurvePoints;

	/** Whether the curve is closed (true) or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Houdini Engine | Public API | Inputs | Input Objects")
	bool bClosed;

	/** Whether the curve is reversed (true) or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Houdini Engine | Public API | Inputs | Input Objects")
	bool bReversed;

	/** The curve type (for example: polygon, nurbs, bezier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Houdini Engine | Public API | Inputs | Input Objects")
	EHoudiniPublicAPICurveType CurveType;

	/** The curve method, for example CVs, or freehand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Houdini Engine | Public API | Inputs | Input Objects")
	EHoudiniPublicAPICurveMethod CurveMethod;

	/** The curve breakpoint parameterization, for example CVs, or freehand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Houdini Engine | Public API | Inputs | Input Objects")
	EHoudiniPublicAPICurveBreakpointParameterization CurveBreakpointParameterization;
	
	/** Helper function for converting EHoudiniPublicAPICurveType to EHoudiniCurveType */
	static EHoudiniCurveType ToHoudiniCurveType(const EHoudiniPublicAPICurveType InCurveType);
	
	/** Helper function for converting EHoudiniPublicAPICurveMethod to EHoudiniCurveMethod */
	static EHoudiniCurveMethod ToHoudiniCurveMethod(const EHoudiniPublicAPICurveMethod InCurveMethod);

	/** Helper function for converting EHoudiniPublicAPICurveBreakpointParameterization to EHoudiniCurveBreakpointParameterization */
	static EHoudiniCurveBreakpointParameterization ToHoudiniCurveBreakpointParamterization(const EHoudiniPublicAPICurveBreakpointParameterization InCurveBreakpointParameterization);
	
	/** Helper function for converting EHoudiniCurveType to EHoudiniPublicAPICurveType */
	static EHoudiniPublicAPICurveType ToHoudiniPublicAPICurveType(const EHoudiniCurveType InCurveType);
	
	/** Helper function for converting EHoudiniCurveMethod to EHoudiniPublicAPICurveMethod */
	static EHoudiniPublicAPICurveMethod ToHoudiniPublicAPICurveMethod(const EHoudiniCurveMethod InCurveMethod);

	/** Helper function for converting EHoudiniCurveBreakpointParameterization to EHoudiniPublicAPIBreakpointParameterization */
	static EHoudiniPublicAPICurveBreakpointParameterization ToHoudiniPublicAPICurveBreakpointParameterization(const EHoudiniCurveBreakpointParameterization InCurveBreakpointParameterization);
};

/**
 * API wrapper input class for curve inputs. Derived from UHoudiniPublicAPIInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPICurveInput : public UHoudiniPublicAPIInput
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPICurveInput();

	/** Indicates that if trigger cook automatically on curve Input spline modified */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bCookOnCurveChanged;

	/** Set this to true to add rot and scale attributes on curve inputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bAddRotAndScaleAttributesOnCurves;

	/** Set this to true to use legacy input curves (i.e. curve::1.0). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bUseLegacyInputCurves;
	
	virtual bool IsAcceptableObjectForInput_Implementation(UObject* InObject) const override;

	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput) override;

	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const override;

protected:
	virtual EHoudiniInputType GetInputType() const override { return EHoudiniInputType::Curve; }

	virtual UObject* ConvertInternalInputObject(UObject* InInternalInputObject) override;

	virtual UObject* ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const override;
};


/**
 * API wrapper input class for asset inputs. Derived from UHoudiniPublicAPIInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIAssetInput : public UHoudiniPublicAPIInput
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPIAssetInput();

	virtual bool IsAcceptableObjectForInput_Implementation(UObject* InObject) const override;

	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput) override;

	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const override;

protected:
	virtual EHoudiniInputType GetInputType() const override { return EHoudiniInputType::Asset; }

	virtual UObject* ConvertInternalInputObject(UObject* InInternalInputObject) override;

	virtual UObject* ConvertAPIInputObjectAndAssignToInput(UObject* InAPIInputObject, UHoudiniInput* InHoudiniInput, const int32 InInputIndex) const override;
};


/**
 * API wrapper input class for world inputs. Derived from UHoudiniPublicAPIGeoInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIWorldInput : public UHoudiniPublicAPIGeoInput
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPIWorldInput();

	/** Objects used for automatic bound selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	TArray<AActor*> WorldInputBoundSelectorObjects;

	/** Indicates that this world input is in "BoundSelector" mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bIsWorldInputBoundSelector;

	/** Indicates that selected actors by the bound selectors should update automatically */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bWorldInputBoundSelectorAutoUpdate;

	/** Resolution used when converting unreal splines to houdini curves */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	float UnrealSplineResolution;

	/**
	 * Setter for world input object array. If this is a bounds selector (#bIsWorldInputBoundSelector is true), then
	 * this function always returns false (and sets nothing), in that case only #WorldInputBoundSelectorObjects can be
	 * modified.
	 * @param InObjects The world input objects.
	 * @return true if all objects in InObjects could be set as world input objects, false otherwise. Always false
	 * if #bIsWorldInputBoundSelector is true.
	 */
	virtual bool SetInputObjects_Implementation(const TArray<UObject*>& InObjects) override;

	virtual bool SupportsTransformOffset() const override { return false; }

	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput) override;

	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const override;

protected:
	virtual EHoudiniInputType GetInputType() const override { return EHoudiniInputType::World; }

};


/**
 * API wrapper input class for landscape inputs. Derived from UHoudiniPublicAPIInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPILandscapeInput : public UHoudiniPublicAPIInput
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPILandscapeInput();

	/** Indicates that the landscape input's source landscape should be updated instead of creating a new component */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bUpdateInputLandscape;

	/** Indicates if the landscape should be exported as heightfield, mesh or points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	EHoudiniLandscapeExportType LandscapeExportType;

	/** Is set to true when landscape input is set to selection only. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bLandscapeExportSelectionOnly;

	/** Is set to true when the automatic selection of landscape component is active */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bLandscapeAutoSelectComponent;

	/** Is set to true when materials are to be exported. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bLandscapeExportMaterials;

	/** Is set to true when lightmap information export is desired. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bLandscapeExportLighting;

	/** Is set to true when uvs should be exported in [0,1] space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bLandscapeExportNormalizedUVs;

	/** Is set to true when uvs should be exported for each tile separately. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Houdini Engine | Public API | Inputs")
	bool bLandscapeExportTileUVs;

	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput) override;

	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const override;

protected:
	virtual EHoudiniInputType GetInputType() const override { return EHoudiniInputType::Landscape; }

};


/**
 * API wrapper input class for geometry collection inputs. Derived from UHoudiniPublicAPIInput.
 */
UCLASS(BlueprintType, Category="Houdini Engine | Public API | Inputs")
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIGeometryCollectionInput : public UHoudiniPublicAPIInput
{
	GENERATED_BODY()

public:
	UHoudiniPublicAPIGeometryCollectionInput();

	/**
	 * Set the transform offset of the specified input object InObject (must already have been set via SetInputObjects()).
	 * @param InObject The input object to set a transform offset for.
	 * @param InTransform The transform offset to set.
	 * @return true if the object was found and the transform offset set.
	 */
	UE_DEPRECATED("1.1.0", "Use SetInputObjectTransformOffset instead.")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs", Meta=(AutoCreateRefTerm="InTransform", DeprecatedFunction, DeprecationMessage="Use SetInputObjectTransformOffset instead."))
	bool SetObjectTransformOffset(UObject* InObject, const FTransform& InTransform);

	/**
	 * Get the transform offset of the specified input object InObject (must already have been set via SetInputObjects()).
	 * @param InObject The input object to get a transform offset for.
	 * @param OutTransform The transform offset that was fetched.
	 * @return true if the object was found and the transform offset fetched.
	 */
	UE_DEPRECATED("1.1.0", "Use GetInputObjectTransformOffset instead.")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs", Meta=(DeprecatedFunction, DeprecationMessage="Use GetInputObjectTransformOffset instead."))
	bool GetObjectTransformOffset(UObject* InObject, FTransform& OutTransform) const;

	/**
	 * Set the transform offset of the input object at the specified index in InputObjects (must already have been set via SetInputObjects()).
	 * @param InInputObjectIndex The input object index to set a transform offset for.
	 * @param InTransform The transform offset to set.
	 * @return true if the index is valid and the transform offset set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs", Meta=(AutoCreateRefTerm="InTransform"))
	bool SetInputObjectTransformOffset(const int32 InInputObjectIndex, const FTransform& InTransform);

	/**
	 * Get the transform offset of the input object at the specified index in InputObjects (must already have been set via SetInputObjects()).
	 * @param InInputObjectIndex The input object index to get a transform offset for.
	 * @param OutTransform The transform offset that was fetched.
	 * @return true if the index is valid and the was transform offset fetched.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool GetInputObjectTransformOffset(const int32 InInputObjectIndex, FTransform& OutTransform) const;

	/**
	 * Get the array of input object transforms.
	 * @param OutInputObjectTransformOffsetArray The output array.
	 * @return false if the input type does not support input object transform offsets. See SupportsTransformOffset.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Inputs")
	bool GetInputObjectTransformOffsetArray(TArray<FTransform>& OutInputObjectTransformOffsetArray) const;

	virtual bool SetInputObjects_Implementation(const TArray<UObject*>& InObjects) override;

	virtual bool PopulateFromHoudiniInput(UHoudiniInput const* const InInput) override;

	virtual bool UpdateHoudiniInput(UHoudiniInput* const InInput) const override;

	virtual void PostLoad() override;

protected:
	virtual bool CopyHoudiniInputObjectPropertiesToInputObject(UHoudiniInputObject const* const InHoudiniInputObject, const int32 InInputObjectIndex) override;

	virtual bool CopyInputObjectPropertiesToHoudiniInputObject(const int32 InInputObjectIndex, UHoudiniInputObject* const InHoudiniInputObject) const override;

	virtual EHoudiniInputType GetInputType() const override { return EHoudiniInputType::Geometry; }

	/** Per-Input-Object data: the transform offset per input object. */
	UE_DEPRECATED("1.1.0", "Use InputObjectTransformOffsetArray via SetInputObjectTransformOffset and GetInputObjectTransformOffset instead")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use InputObjectTransformOffsetArray instead"))
	TMap<UObject*, FTransform> InputObjectTransformOffsets_DEPRECATED;
	
private:
	/** Transform offset per input object (by input object array index). */
	UPROPERTY()
	TArray<FTransform> InputObjectTransformOffsetArray;
};
