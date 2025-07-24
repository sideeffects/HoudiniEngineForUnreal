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

#include "UObject/ObjectMacros.h"
#include "HoudiniCookable.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniPCGDataObject.h"
#include "PCGComponent.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniPCGCookable.generated.h"

class UHoudiniPCGDataObject;
class UHoudiniPCGDataCollection;
struct FHoudiniPCGObjectOutput;
class UPCGData;
class UPCGMetadata;
struct FPCGContext;
class UHoudiniPCGSettings;
class UHoudiniPCGComponent;
class UHoudiniPCGManagedResource;

enum class EPCGCookableState
{
	None,				// Create, but nothing happening.
	Loaded,				// Loaded, but not loaded into Houdini.
	WaitingForSession,	// Waiting for Houdini Session to be created.
	Initializing,		// Cookable is being loaded into Houdini for the first time.
	Initialized,		// Cookable has been loaded into Houdini. Parameters/Inputs can be accessed.

	// Typically Setting parameters and inputs occurs between Initialized and Cooked.

	Cooking,			// Cookable is cooking.
	CookingComplete		// Cookable is done cooking and outputs have been processed.
};

UCLASS()
class UHoudiniPDGBakeOutput : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FHoudiniBakedOutput> BakedOutputs;
};

UCLASS()
class HOUDINIENGINE_API UHoudiniPCGCookable  : public UObject
{
	// This class wraps a single UHoudiniCookable for use in PCG. It contains additional state
	// and information to link it to the PCG classes. Its used in two circumstances.
	//
	// 1. Each UHoudiniPCGSettings contains a FHoudiniPCGCookable which is used
	//		to obtain parameter, input and output information about the HDA. Its results
	//		(cooked or baked) are never used, its just used for determining inputs and outputs in the
	//		PCG Graph editor.
	//
	//	2. A FHoudiniPCGCookable is created for each PCG node that executes. FHoudiniPCGCookables
	//		are re-used between executions to improve performance. Additionally one FHoudiniPCGCookable
	//		may be created for each execution in a loop. 
	//
	
	GENERATED_BODY()
public:
	UHoudiniPCGCookable(const FObjectInitializer& ObjectInitializer);
	virtual ~UHoudiniPCGCookable() override;

	// Instantiates a new HDA... instantiating is asynchronous.
	void CreateHoudiniCookable(UHoudiniAsset* Asset, UHoudiniPCGSettings* PCGSettings, UHoudiniPCGComponent* Component);

	// Instantiates a new HDA... instantiating is asynchronous.
	void Instantiate();

	// UpdateParametersAndInputs() pulls the inputs and parameters from the context and cooks the Houdini Cookable.
	// Returns true if succeeded, fals if failed. bParamsChanged && bInputsChanged are updated.
	bool UpdateParametersAndInputs(FPCGContext* Context);

	void StartCook();
	bool NeedsCook() const;

	void CopyParametersAndInputs(const UHoudiniPCGCookable * Other);

	// DestroyCookable() releases() all data associated with the cook.
	void DestroyCookable(UWorld * World);

	// Updates the current cookable state.
	void Update(FPCGContext* Context);

	// Bake the Cookable
	void Bake();

	void Rebuild();

	void PostLoad();
	void PostEditImport();

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bAutomaticallyDeleteAssets = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UHoudiniCookable> Cookable;

	UPROPERTY()
	TObjectPtr<UHoudiniPDGBakeOutput> PDGBakedOutput;

	EPCGCookableState State = EPCGCookableState::None;
	bool bIsCookingPDG = false;

	bool bParamsChanged = false;
	bool bInputsChanged = false;

	void ProcessCookedOutput(FPCGContext* Context);
	void ProcessBakedOutput(FPCGContext* Context);

	void DeleteBakedOutput(UWorld* World);

	void AddCookError(const FString& Error) { Errors.Add(Error); }
	const TArray<FString>& GetErrors() { return Errors;  }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostOutputProcessingDelegate, UHoudiniPCGCookable*, bool);

	FOnPostOutputProcessingDelegate OnInitializedDelegate;
	FOnPostOutputProcessingDelegate OnPostOutputProcessingDelegate;

private:

	TArray<FSoftObjectPath> TrackedObjects;
	int CookCount = -1;
	FDelegateHandle PDGTopNetworkCookedDelegate;
	TArray<FString> Errors;

	void ProcessCookedOutput(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput);

	void ProcessBakedOutputs(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const FHoudiniBakedOutput* HoudiniOutput);

	static void CreateOutputPinFromCookedData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput);
	static void CreateOutputPinFromBakedData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const FHoudiniBakedOutput* HoudiniOutput);
	static void CreateOutputPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const TArray<FHoudiniPCGObjectOutput> & Outputs);
	static void CopyCookedPCGOutputDataToPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutput);
	static void CopyBakedPCGOutputDataToPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const FHoudiniBakedOutput* HoudiniOutput);

	static void CopyPCGOutputDataToPinData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniPCGOutputData* PCGOutput);

	bool ApplyInputsToCookable(const FPCGContext* InContext);

	bool ApplyParametersToCookable(const FPCGContext* Context);

	bool ApplyParametersToCookable(const UPCGData* Data);

	void OnCookingComplete(bool bSuccess);

	void OnCookingCompleteInternal(bool bSuccess);

	void InvalidateCookable();

	void AddTrackedObjects(const FPCGContext* Context);

	static bool ApplyInputAsUnrealObjects(const FPCGContext* Context, UHoudiniInput* HoudiniInput, const TArray<FString> & InputObjects);

	static UHoudiniPCGDataObject* GetPCGDataObjects(const FPCGTaggedData& TaggedData);

	TArray<FString> GetUnrealObjectPaths(const FPCGContext* Context, const UPCGMetadata* Metadata);

	static bool ApplyInputAsPCGData(UHoudiniInput* HoudiniInput, const TArray<UHoudiniPCGDataCollection*> & PCGCollections);

	static void DeleteBakedActor(const FString& ActorPath);
	static void DeleteBakedComponent(const FString& ActorPath);
	static void DeleteBakedObject(const FString& ObjectPath);
	static void DeletePackage(UPackage* Package);
	static void DeleteLandscapeLayer(const FString & LandscapePath, TArray<FString>& LandscapeLayers);
	static void DeleteFoliage(UWorld* World, UFoliageType* FoliageType, const TArray<FVector>& FoliageInstancePositions);
	void DeleteBakedOutputObject(UWorld* World, FHoudiniBakedOutputObject& BakedOutputObject);
};



