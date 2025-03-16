/*
* Copyright (c) <2025> Side Effects Software Inc.
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
#include "HoudiniPCGCookable.h"
#include "PCGComponent.h"
#include "UObject/ObjectMacros.h"
#include "IDetailCustomization.h"
#include "HoudiniPCGComponent.generated.h"


UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, Activation, "Components|Activation"), ShowCategories = (Mobility), editinlinenew)
class HOUDINIENGINE_API UHoudiniPCGComponent : public USceneComponent
{
public:
	// This component is attached to an AHoudiniPCGActor which stores generated cooked data during PCG Cooks.
	GENERATED_UCLASS_BODY()

	void OnComponentDestroyed(bool bDestroyingHierarchy) override;
public:

	static UHoudiniPCGComponent* CreatePCGComponent(UPCGComponent* UnrealPCGComponent);

	UPROPERTY()
	TObjectPtr<UHoudiniPCGCookable> Cookable;

	UPROPERTY()
	TObjectPtr<UHoudiniAsset> HoudiniAsset;

	UPROPERTY()
	TWeakObjectPtr<UPCGComponent> PCGComponent;
};

class UHoudiniPCGComponentDetails  : public IDetailCustomization
{
public:
	UHoudiniPCGComponentDetails();

	// Factory method to register the customization
	static TSharedRef<IDetailCustomization> MakeInstance();

	// Override to customize the details panel
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

UCLASS()
class HOUDINIENGINE_API AHoudiniPCGActor : public AActor
{
public:
	GENERATED_BODY()

};