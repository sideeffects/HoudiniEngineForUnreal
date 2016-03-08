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

#pragma once
#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterRamp.generated.h"


class UCurveBase;
class UHoudiniAssetParameterRamp;
class UHoudiniAssetParameterFloat;
class UHoudiniAssetParameterColor;
class UHoudiniAssetParameterChoice;
class SHoudiniAssetParameterRampCurveEditor;


UCLASS(BlueprintType)
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterRampCurveFloat : public UCurveFloat
{
	GENERATED_UCLASS_BODY()

public:

	/** Set parent ramp parameter. **/
	void SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp);

/** FCurveOwnerInterface methods. **/
public:

#if WITH_EDITOR

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;

#endif

protected:

	/** Parent ramp parameter. **/
	UHoudiniAssetParameterRamp* HoudiniAssetParameterRamp;
};


namespace EHoudiniAssetParameterRampCurveColorEvent
{
	enum Type
	{
		None = 0,
		MoveStop,
		ChangeStopTime,
		ChangeStopColor,
		AddStop,
		RemoveStop
	};
}


UCLASS(BlueprintType)
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterRampCurveColor : public UCurveLinearColor,
	public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Set parent ramp parameter. **/
	void SetParentRampParameter(UHoudiniAssetParameterRamp* InHoudiniAssetParameterRamp);

	/** Return the current type of event. **/
	EHoudiniAssetParameterRampCurveColorEvent::Type GetColorEvent() const;

	/** Reset the current type of event. **/
	void ResetColorEvent();


/** FCurveOwnerInterface methods. **/
public:

#if WITH_EDITOR

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;

#endif

/** UObject methods. **/
public:

	virtual bool Modify(bool bAlwaysMarkDirty);

/** FTickableGameObject methods. **/
public:

	virtual bool IsTickableInEditor() const;
	virtual bool IsTickableWhenPaused() const;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

protected:

	/** Attempt to map current editor transaction type to curve transactions. **/
	EHoudiniAssetParameterRampCurveColorEvent::Type GetEditorCurveTransaction() const;

protected:

	/** Parent ramp parameter. **/
	UHoudiniAssetParameterRamp* HoudiniAssetParameterRamp;

	/** Current event. **/
	EHoudiniAssetParameterRampCurveColorEvent::Type ColorEvent;
};


namespace EHoudiniAssetParameterRampKeyInterpolation
{
	enum Type
	{
		Constant = 0,
		Linear,
		CatmullRom,
		MonotoneCubic,
		Bezier,
		BSpline,
		Hermite
	};
}


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterRamp : public UHoudiniAssetParameterMultiparm
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor. **/
	virtual ~UHoudiniAssetParameterRamp();

public:

	/** Create instance of this class. **/
	static UHoudiniAssetParameterRamp* Create(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo);

public:

	/** Create this parameter from HAPI information. **/
	virtual bool CreateParameter(UHoudiniAssetComponent* InHoudiniAssetComponent,
		UHoudiniAssetParameter* InParentParameter, HAPI_NodeId InNodeId, const HAPI_ParmInfo& ParmInfo) override;

	/** Notification from component that all child parameters have been created. **/
	virtual void NotifyChildParametersCreated();

/** UObject methods. **/
public:

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR

	/** Create widget for this parameter and add it to a given category. **/
	virtual void CreateWidget(IDetailCategoryBuilder& DetailCategoryBuilder) override;

#endif

	/** Called when curve editing is finished and update should take place. **/
	void OnCurveEditingFinished();

	/** Called when float ramp parameter changes via user interface. **/
	void OnCurveFloatChanged(UHoudiniAssetParameterRampCurveFloat* CurveFloat);

	/** Called when color ramp parameter changes via user interface. **/
	void OnCurveColorChanged(UHoudiniAssetParameterRampCurveColor* CurveColor);

protected:

	/** Populate curve with point data. **/
	void GenerateCurvePoints();

	/** Return number of ramp keys. **/
	int32 GetRampKeyCount() const;

	/** Translate choice value into interpolation enumeration. **/
	EHoudiniAssetParameterRampKeyInterpolation::Type
		TranslateChoiceKeyInterpolation(UHoudiniAssetParameterChoice* ChoiceParam) const;

	/** Return Unreal ramp key interpolation type from Houdini ramp key interpolation type. **/
	ERichCurveInterpMode
		TranslateHoudiniRampKeyInterpolation(EHoudiniAssetParameterRampKeyInterpolation::Type KeyInterpolation) const;

	/** Return Houdini ramp key interpolation type from Unreal ramp key interpolation type. **/
	EHoudiniAssetParameterRampKeyInterpolation::Type
		TranslateUnrealRampKeyInterpolation(ERichCurveInterpMode RichCurveInterpMode) const;

	/** Retrieve ramp key parameters for a given index of a float ramp. **/
	bool GetRampKeysCurveFloat(int32 Idx, UHoudiniAssetParameterFloat*& Position, UHoudiniAssetParameterFloat*& Value,
		UHoudiniAssetParameterChoice*& Interp) const;

	/** Retrieve ramp key parameters for a given index of a color ramp. **/
	bool GetRampKeysCurveColor(int32 Idx, UHoudiniAssetParameterFloat*& Position, UHoudiniAssetParameterColor*& Value,
		UHoudiniAssetParameterChoice*& Interp) const;

protected:

	//! Default spline interpolation method.
	static const EHoudiniAssetParameterRampKeyInterpolation::Type DefaultSplineInterpolation;

	//! Default unknown interpolation method.
	static const EHoudiniAssetParameterRampKeyInterpolation::Type DefaultUnknownInterpolation;

protected:

#if WITH_EDITOR

	//! Curve editor widget.
	TSharedPtr<SHoudiniAssetParameterRampCurveEditor> CurveEditor;

#endif

	//! Curves which are being edited.
	UHoudiniAssetParameterRampCurveFloat* HoudiniAssetParameterRampCurveFloat;
	UHoudiniAssetParameterRampCurveColor* HoudiniAssetParameterRampCurveColor;

	//! Set to true if this ramp is a float ramp. Otherwise is considered a color ramp.
	bool bIsFloatRamp;

	//! Set to true if the curve has changed through Slate interaction.
	bool bIsCurveChanged;

	//! Set to true when curve data needs to be re-uploaded to Houdini Engine.
	bool bIsCurveUploadRequired;
};
