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
#include "PCGComponent.h"
#include "HoudiniPCGCookable.generated.h"

class UHoudiniPCGDataObject;
class UHoudiniPCGDataCollection;
struct FHoudiniPCGObjectOutput;
class UPCGData;
class UPCGMetadata;
struct FPCGContext;
class UHoudiniDigitalAssetPCGSettings;
class UHoudiniPCGComponent;
class UHoudiniPCGManagedResource;

enum class EPCGCookableState
{
	Initializing,		// Cookable is being loaded into Houdini
	Initialized,		// Cookable has been loaded into Houdini. Parameters/Inputs can be accessed.
	Idle,				// Doing nothing.
	Cooking,			// Cookable is cooking.
	Done				// Cookable is done cookiing and outputs have been processed.
};

UCLASS()
class HOUDINIENGINE_API UHoudiniPCGCookable  : public UObject
{
	// This class wraps a single UHoudiniCookable for use in PCG. It contains additional state
	// and information to link it to the PCG classes. Its used in two circumstances.
	//
	// 1. Each UHoudiniDigitalAssetPCGSettings contains a FHoudiniPCGCookable which is used
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
	void Instantiate(UHoudiniAsset* Asset, UHoudiniDigitalAssetPCGSettings * PCGSettings, UHoudiniPCGComponent * Component);

	// UpdateAndCook() pulls the inputs and parameters from the context and cooks the Houdini Cookable.
	bool UpdateAndCook(FPCGContext* Context, bool& bError);

	// Release() releases() all data associated with the cook.
	void Release();

	// Updates the current cookable state.
	bool Update(FPCGContext* Context, bool& bError);

private:

	UPROPERTY()
	TObjectPtr<UHoudiniCookable> Cookable;

	UPROPERTY()
	TObjectPtr<UPCGComponent> PCGComponent;

	EPCGCookableState State = EPCGCookableState::Idle;
	TArray<FSoftObjectPath> TrackedObjects;
	int CookCount = -1;

	static void CreateOutputsAsObjectReferences(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const TArray<FHoudiniPCGObjectOutput> & Outputs);
	static void CreateOutputsAsPCGData(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutputs);

	bool ApplyInputsToCookable(FPCGContext* InContext, bool& bErrors);

	bool ApplyParametersToCookable(FPCGContext* Context, bool & bErrors);

	bool ApplyParametersToCookable(const UPCGData* Data, FPCGContext* Context, bool & bErrors);

	void OnCookingComplete(bool bSuccess);

	void InvalidateCookable();

	void ProcessCookableOutput(FPCGContext* Context);

	void AddTrackedObjects(FPCGContext* Context);

	bool ApplyInputAsUnrealObjects(FPCGContext* Context, UHoudiniInput* HoudiniInput, const TArray<FString> & InputObjects, bool& bErrors);

	UHoudiniPCGDataObject* GetPCGDataObjects(FPCGContext* Context, const FPCGTaggedData& TaggedData);

	TArray<FString> GetUnrealObjectPaths(FPCGContext* Context, const UPCGMetadata* Metadata, bool& bError);

	bool ApplyInputAsPCGData(FPCGContext* Context, UHoudiniInput* HoudiniInput, const TArray<UHoudiniPCGDataCollection*> & PCGCollections);

	void CreateOutputs(FPCGContext* Context, const FName& OutputPinName, const FString& TagName, const UHoudiniOutput* HoudiniOutputs);
};



