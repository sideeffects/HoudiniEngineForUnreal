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
#include "Components/MeshComponent.h"

#include "HoudiniStaticMeshComponent.generated.h"

class UHoudiniStaticMesh;
class UBillboardComponent;

UCLASS(EditInlineNew, ClassGroup = "Houdini Engine | Rendering")
class HOUDINIENGINERUNTIME_API UHoudiniStaticMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

	friend class FHoudiniEditorEquivalenceUtils;

public:
	UHoudiniStaticMeshComponent(const FObjectInitializer &InInitialzer);

	UFUNCTION()
	void SetMesh(UHoudiniStaticMesh *InMesh);

	UFUNCTION()
	UHoudiniStaticMesh* GetMesh() { return Mesh; }

	// Call this if the mesh updated (outside of calling SetMesh).
	UFUNCTION()
	void NotifyMeshUpdated();
	
	virtual void OnRegister() override;

	//virtual void PostLoad() override;

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const override;
	virtual TArray<FName> GetMaterialSlotNames() const override;
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const override;
	// end - UPrimitiveComponent interface

	// USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	// end - USceneComponent Interface.

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	bool IsHoudiniIconVisible() const { return bHoudiniIconVisible; }

	UFUNCTION()
	void SetHoudiniIconVisible(bool bInHoudiniIconVisible);

protected:
#if WITH_EDITORONLY_DATA
	virtual void UpdateSpriteComponent();
#endif

	/** The mesh. */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	UHoudiniStaticMesh *Mesh;

	/** Local space bounds of mesh. */
	UPROPERTY()
	FBox LocalBounds;

	UPROPERTY(EditAnywhere, Category = "Icons")
	bool bHoudiniIconVisible;

};