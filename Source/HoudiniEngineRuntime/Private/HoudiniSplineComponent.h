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

#include "HoudiniEngineCopyPropertiesInterface.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "HoudiniGeoPartObject.h"

#include "HoudiniSplineComponent.generated.h"

class UHoudiniAssetComponent;

enum class EHoudiniCurveType : int8;

enum class EHoudiniCurveMethod : int8;

class UHoudiniInputObject;

UCLASS(Blueprintable, BlueprintType, EditInlineNew, config = Engine, meta = (BlueprintSpawnableComponent))
class HOUDINIENGINERUNTIME_API UHoudiniSplineComponent : public USceneComponent, public IHoudiniEngineCopyPropertiesInterface
{
	GENERATED_UCLASS_BODY()

	friend class UHoudiniSplineComponent_V1;

	friend class FHoudiniEditorEquivalenceUtils;

	virtual ~UHoudiniSplineComponent();

	virtual void Serialize(FArchive & Ar) override;

	public:

		void Construct(TArray<FVector>& InCurveDisplayPoints, int32 InsertedPoint = -1);

		void CopyHoudiniData(const UHoudiniSplineComponent* OtherHoudiniSplineComponent);

		void ResetCurvePoints();

		void ResetDisplayPoints();

		void AddCurvePoints(const TArray<FTransform>& Points);

		void AddDisplayPoints(const TArray<FVector>& Points);

		void AppendPoint(const FTransform& NewPoint);

		void InsertPointAtIndex(const FTransform& NewPoint, const int32& Index);

		void RemovePointAtIndex(const int32& Index);

		void EditPointAtindex(const FTransform& NewPoint, const int32& Index);

		void MarkModified(const bool & InModified) { bHasChanged = InModified; };

		// To set the offset of default position of houdini curve
		void SetOffset(const float& Offset);

		bool HasChanged() const;

		void MarkChanged(const bool& Changed);

		// Mark the associated Houdini nodes for pending kill
		void MarkInputNodesAsPendingKill();

		FORCEINLINE
		FString& GetHoudiniSplineName() { return HoudiniSplineName; }

		FORCEINLINE
		void SetHoudiniSplineName(const FString& NewName) { HoudiniSplineName = NewName; }

		bool NeedsToTriggerUpdate() const;

		void SetNeedsToTriggerUpdate(const bool& NeedsToTriggerUpdate);

		FORCEINLINE
		EHoudiniCurveType GetCurveType() const { return CurveType; }

		void SetCurveType(const EHoudiniCurveType& NewCurveType);

		FORCEINLINE
		EHoudiniCurveMethod GetCurveMethod() const { return CurveMethod; }

		FORCEINLINE
		void SetCurveMethod(const EHoudiniCurveMethod& NewCurveMethod) { CurveMethod = NewCurveMethod; }

		FORCEINLINE
		EHoudiniCurveBreakpointParameterization GetCurveBreakpointParameterization() const { return CurveBreakpointParameterization; }
	
		FORCEINLINE
		void SetCurveBreakpointParameterization(const EHoudiniCurveBreakpointParameterization& NewBreakpointMethod) { CurveBreakpointParameterization = NewBreakpointMethod; }
	
		FORCEINLINE
		int32 GetCurvePointCount() const { return CurvePoints.Num(); }

		FORCEINLINE
		bool IsClosedCurve() const { return bClosed; }

		FORCEINLINE
		void SetClosedCurve(const bool& Closed) { bClosed = Closed; }

		FORCEINLINE
		bool IsReversed() const { return bReversed; }

		void SetReversed(const bool& Reversed);

		FORCEINLINE
		int32 GetCurveOrder() const { return CurveOrder; }
	
		void SetCurveOrder(const int32& Order) { CurveOrder = Order; }

		FORCEINLINE
		bool IsInputCurve() const { return bIsInputCurve; }

		FORCEINLINE
		void SetIsInputCurve(const bool& bIsInput) { bIsInputCurve = bIsInput; }

		FORCEINLINE
		bool IsEditableOutputCurve() const { return bIsEditableOutputCurve; }
		
		FORCEINLINE
		void SetIsEditableOutputCurve(const bool& bInIsEditable) { bIsEditableOutputCurve = bInIsEditable; };

		FORCEINLINE
		int32 GetNodeId() const { return NodeId; }

		FORCEINLINE
		void SetNodeId(const int32& NewNodeId) { NodeId = NewNodeId; }

		FORCEINLINE
		FString GetGeoPartName() const { return PartName; }

		FORCEINLINE
		bool IsHoudiniSplineVisible() const { return bIsHoudiniSplineVisible; }

		FORCEINLINE
		void SetHoudiniSplineVisible(bool Visible) { bIsHoudiniSplineVisible = Visible; }

		FORCEINLINE
		void SetGeoPartName(const FString & InPartName) { PartName = InPartName; }

		FORCEINLINE
		bool IsLegacyInputCurve() const { return bIsLegacyInputCurve; }
	
		FORCEINLINE
		void SetIsLegacyInputCurve(const bool IsLegacyInputCurve) { bIsLegacyInputCurve = IsLegacyInputCurve; }
	
		virtual void OnUnregister() override;

		virtual void OnComponentCreated() override;

		virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

#if WITH_EDITOR
		virtual void PostEditUndo() override;
		virtual void PostEditChangeProperty(FPropertyChangedEvent & PeopertyChangedEvent) override;
#endif

		virtual void PostLoad() override;

		virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
		void ApplyComponentInstanceData(struct FHoudiniSplineComponentInstanceData* ComponentInstanceData, const bool bPostUCS);

		virtual void CopyPropertiesFrom(UObject* FromObject) override;

	private:

		void ReverseCurvePoints();

	public:

		UPROPERTY()
		TArray<FTransform> CurvePoints;

		UPROPERTY()
		TArray<FVector> DisplayPoints;

		UPROPERTY()
		TArray<int32> DisplayPointIndexDivider;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		FString HoudiniSplineName;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		bool bClosed;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		bool bReversed;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		int32 CurveOrder;
	
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		bool bIsHoudiniSplineVisible;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		EHoudiniCurveType CurveType;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		EHoudiniCurveMethod CurveMethod;

		// Only used for new HAPI curve / breakpoints
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini Spline Properties")
		EHoudiniCurveBreakpointParameterization CurveBreakpointParameterization;
	
		UPROPERTY()
		bool bIsOutputCurve;

		UPROPERTY()
		bool bCookOnCurveChanged;

		UPROPERTY()
		bool bIsLegacyInputCurve;
	
	
#if WITH_EDITORONLY_DATA
		UPROPERTY()
		TArray<int32> EditedControlPointsIndexes;

		UPROPERTY(NonTransactional)
		bool bPostUndo;
#endif

	protected:
		// Corresponding geo part object.
		FHoudiniGeoPartObject HoudiniGeoPartObject;
	
	private:
		UPROPERTY(Transient, DuplicateTransient)
		bool bHasChanged;

		UPROPERTY(Transient, DuplicateTransient)
		bool bNeedsToTriggerUpdate;

		// Whether this is a Houdini curve input
		UPROPERTY()
		bool bIsInputCurve;

		UPROPERTY()
		bool bIsEditableOutputCurve;

		// Corresponds to the Curve NodeId in Houdini
		UPROPERTY(Transient, DuplicateTransient)
		int32 NodeId;

		UPROPERTY()
		FString PartName;
};

// Used to store HoudiniAssetComponent data during BP reconstruction
USTRUCT()
struct FHoudiniSplineComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()
public:

	FHoudiniSplineComponentInstanceData();
	FHoudiniSplineComponentInstanceData(const UHoudiniSplineComponent* SourceComponent);
	
	virtual ~FHoudiniSplineComponentInstanceData() = default;

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		Super::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UHoudiniSplineComponent>(Component)->ApplyComponentInstanceData(this, (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript));
	}

	UPROPERTY()
	TArray<FTransform> CurvePoints;

	UPROPERTY()
	TArray<FVector> DisplayPoints;

	UPROPERTY()
	TArray<int32> DisplayPointIndexDivider;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<int32> EditedControlPointsIndexes;
#endif
	
};
