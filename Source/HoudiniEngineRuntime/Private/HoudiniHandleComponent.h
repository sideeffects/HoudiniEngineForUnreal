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

#include "Components/SceneComponent.h"

#include "HoudiniHandleComponent.generated.h"

class UHoudiniParameter;

UENUM()
enum class EXformParameter : uint8
{
	TX, TY, TZ,
	RX, RY, RZ,
	SX, SY, SZ,
	COUNT
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniHandleParameter : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UHoudiniParameter* AssetParameter;

	UPROPERTY()
	int32 TupleIndex;

	
	bool Bind(
	    float & OutValue,
		const char * CmpName,
		int32 InTupleIdx,
		const FString & HandleParmName,
		UHoudiniParameter* Parameter);

	bool Bind(
		TSharedPtr<FString> & OutValue,
		const char * CmpName,
		int32 InTupleIdx,
		const FString & HandleParmName,
		UHoudiniParameter* Parameter);

	TSharedPtr<FString> Get(TSharedPtr<FString> DefaultValue) const;

	UHoudiniHandleParameter & operator=(float Value);

};

UENUM()
enum class EHoudiniHandleType : uint8 
{
	Xform,
	Bounder,
	Unsupported
};

UCLASS(Blueprintable, BlueprintType, EditInlineNew, config = Engine, meta = (BlueprintSpawnableComponent))
class HOUDINIENGINERUNTIME_API UHoudiniHandleComponent : public USceneComponent 
{
public:	

	friend class UHoudiniAssetComponent;

	friend class FHoudiniHandleComponentVisualizer;

	friend class FHoudiniEditorEquivalenceUtils;

	GENERATED_UCLASS_BODY()

	virtual void Serialize(FArchive & Ar) override;

	FString GetHandleName() const { return HandleName; };
	EHoudiniHandleType GetHandleType() const { return HandleType; };

	void SetHandleName(const FString& InHandleName) { HandleName = InHandleName; };
	void SetHandleType(const EHoudiniHandleType& InHandleType) { HandleType = InHandleType; };

	// Equality, consider two handle equals if they have the same name, type, tuple size and disabled status
	bool operator==(const UHoudiniHandleComponent& other) const
	{
		return (HandleType == other.HandleType && HandleName.Equals(other.HandleName));
	}

	bool Matches(const UHoudiniHandleComponent& other) const { return (*this == other); };

	void InitializeHandleParameters();

	bool CheckHandleValid() const;

	FBox GetBounds() const;

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	bool IsTransformUpdateNeeded();

public:
	UPROPERTY()
	TArray<UHoudiniHandleParameter*> XformParms;

	UPROPERTY()
	UHoudiniHandleParameter* RSTParm;

	UPROPERTY()
	UHoudiniHandleParameter* RotOrderParm;

	UPROPERTY()
	FTransform LastSentTransform;

	bool bNeedToUpdateTransform;

private:
	UPROPERTY()
	EHoudiniHandleType HandleType;

	UPROPERTY()
	FString HandleName;

	double dLastTransformUpdateTime;
};
