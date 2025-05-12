/* Copyright (c) <2025> Side Effects Software Inc.
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
#include "HoudiniOutput.h"
#include "HoudiniPCGCookable.h"
#include "PCGManagedResource.h"
#include "GameFramework/Actor.h"
#include "HoudiniPCGManagedResource.generated.h"

class UHoudiniPCGComponent;

UCLASS(BlueprintType)
class HOUDINIENGINE_API UHoudiniPCGManagedResource : public UPCGManagedResource
{
	// This class is derived from a built on PCG class UPCGManagedResource and is used to keep track
	// of HDA output data.

	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostEditImport() override;
	//~End UObject interface

	//~Begin UPCGManagedResource interface
	virtual void PostApplyToComponent() override;
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool MoveResourceToNewActor(AActor* NewActor) override;
	virtual void MarkAsUsed() override;
	virtual void MarkAsReused() override;
	virtual void PostLoad() override;

	void DestroyCookable();

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode) override;
#endif
	//~End UPCGManagedResource interface

	void OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);

	UPROPERTY()
	TObjectPtr<UHoudiniPCGComponent> HoudiniPCGComponent;

	UPROPERTY()
	TObjectPtr<UPCGComponent> PCGComponent;

	UPROPERTY()
	bool bInvalidateResource = false;

};

