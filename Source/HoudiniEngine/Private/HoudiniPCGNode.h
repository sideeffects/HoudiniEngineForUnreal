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
	Cook = 1,
	CookAndBake,
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class HOUDINIENGINE_API UHoudiniPCGSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void PopulateInputsAndOutputs();

	virtual bool CanCullTaskIfUnwired() const { return false; }
	virtual void BeginDestroy() override;

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

	void ResetFromHDA();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HoudiniPCG)
	TObjectPtr<UHoudiniAsset> HoudiniAsset;

	/** By default, data table loading is asynchronous, can force it synchronous if needed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Properties
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Keep track of number of inputs... don't just used the cookable value so we don't break connections when the number
	// of inputs changes.
	UPROPERTY()
	int NumInputs = 0;

	UPROPERTY(EditAnywhere, Category = HoudiniPCG)
	EHoudiniPCGOutputType OutputType = EHoudiniPCGOutputType::CookAndBake;
	UPROPERTY(EditAnywhere, Category = HoudiniPCG)
	bool bCreateSceneComponents = true;

	UPROPERTY(EditAnywhere, Category = HoudiniPCG)
	bool bAutomaticallyDeleteTempAssets = true;

	UPROPERTY(EditAnywhere, Category = HoudiniPCG)
	bool bUsePCGCache = true;

	UPROPERTY()
	int64 IterationCount = 0; // dummy value to keep track of changes.

	FName GetOutputPinName() const;

	UPROPERTY(Instanced)
	TObjectPtr<UHoudiniPCGCookable> ParameterCookable;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

	void InstantiateParameterCookable();
	void InstantiateNewParameterCookable();
	void OnParameterCookableCooked();
	void OnParameterCookableInitialized();

	void PostEditImport() override;
	void SetupCookable();
	void ForceRefreshUI();
	void SetNodeLabelPrefix();
};

enum class EHoudiniPCGContextState
{
	None,
	Instantiating,
	Cooking,
	Done
};
struct FPCHoudiniDigitalAssetAttributesContext : public FPCGContext, public IPCGAsyncLoadingContext
{
public:
	EHoudiniPCGContextState ContextState = EHoudiniPCGContextState::None;

};

class FHoudiniDigitalAssetPCGElement : public IPCGElementWithCustomContext<FPCHoudiniDigitalAssetAttributesContext>
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* InContext) const override { return true; }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
#endif
	FPCGCrc SetCrc(FPCGContext* Context) const;

protected:
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	virtual void AbortInternal(FPCGContext* Context) const;
};

