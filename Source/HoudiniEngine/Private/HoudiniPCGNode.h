/*
* Copyright (c) <2025> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once
#include "HoudiniAsset.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "Async/PCGAsyncLoadingContext.h"
#include "HoudiniCookable.h"
#include "HoudiniPCGCookable.h"
#include "HoudiniPCGNode.generated.h"

UENUM()
enum class EHoudiniPCGOutputType : uint8
{
	Ignore = 0,
	Cook,
	Bake,
};

USTRUCT()
struct FHoudiniPCGOutput
{
	GENERATED_BODY()
public:
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UHoudiniDigitalAssetPCGSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void PopulateInputsAndOutputs();

	virtual bool CanCullTaskIfUnwired() const { return false; }

#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("HoudiniDigitalAsset")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("HoudiniDigitalAsset", "NodeTitle", "Houdini Digital Asset"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::InputOutput; }
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
	virtual bool HasDynamicPins() const { return true; }
#endif

	virtual FString GetAdditionalTitleInformation() const override;

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TObjectPtr<UHoudiniAsset> HoudiniAsset;

	/** By default, data table loading is asynchronous, can force it synchronous if needed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Properties
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Geometry, meta = (PCG_Overridable))
	TArray<TObjectPtr<UObject>> Inputs;

	UPROPERTY(EditAnywhere, Category = Geometry, meta = (PCG_Overridable))
	TArray<FHoudiniPCGOutput> Outputs;

	UPROPERTY(EditAnywhere)
	EHoudiniPCGOutputType OutputType = EHoudiniPCGOutputType::Bake;

	UPROPERTY(EditAnywhere)
	bool bExposeParameters = true;

	UPROPERTY(EditAnywhere)
	bool bForceCookOnDirty = true;

	FName GetOutputPinName(int Index) const;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

	void InstantiatePCGEditorHDA();

	UPROPERTY(Transient)
	TObjectPtr<UHoudiniCookable> ParameterCookable;
};

struct FPCHoudiniDigitalAssetAttributesContext : public FPCGContext, public IPCGAsyncLoadingContext {};

class FHoudiniDigitalAssetPCGElement : public IPCGElementWithCustomContext<FPCHoudiniDigitalAssetAttributesContext>
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
protected:
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;

	void ProcessCookableOutput(FPCGContext* Context, UHoudiniPCGCookable* Cookable) const;
};

