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

#include "HoudiniInputObject.h"

#include "HoudiniEngineRuntime.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniInput.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DataTable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Landscape.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "GameFramework/Volume.h"
#include "Camera/CameraComponent.h"
#include "FoliageType_InstancedStaticMesh.h"

#include "Model.h"
#include "Engine/Brush.h"

#include "HoudiniEngineRuntimeUtils.h"
#include "Kismet/KismetSystemLibrary.h"

#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"

//-----------------------------------------------------------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------------------------------------------------------

//
UHoudiniInputObject::UHoudiniInputObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Transform(FTransform::Identity)
	, Type(EHoudiniInputObjectType::Invalid)
	, InputNodeId(-1)
	, InputObjectNodeId(-1)
	, bHasChanged(false)
	, bNeedsToTriggerUpdate(false)
	, bTransformChanged(false)
	, bImportAsReference(false)
	, bCanDeleteHoudiniNodes(true)
{
	Guid = FGuid::NewGuid();
}

//
UHoudiniInputStaticMesh::UHoudiniInputStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputSkeletalMesh::UHoudiniInputSkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputGeometryCollection::UHoudiniInputGeometryCollection(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
{

}

//
UHoudiniInputGeometryCollectionComponent::UHoudiniInputGeometryCollectionComponent(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
{
}

//
UHoudiniInputGeometryCollectionActor::UHoudiniInputGeometryCollectionActor(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
{
}

//
UHoudiniInputSceneComponent::UHoudiniInputSceneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputMeshComponent::UHoudiniInputMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputInstancedMeshComponent::UHoudiniInputInstancedMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputSplineComponent::UHoudiniInputSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NumberOfSplineControlPoints(-1)
	, SplineLength(-1.0f)
	, SplineResolution(-1.0f)
	, SplineClosed(false)
{

}

//
UHoudiniInputCameraComponent::UHoudiniInputCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FOV(0.0f)
	, AspectRatio(1.0f)
	, bIsOrthographic(false)
	, OrthoWidth(2.0f)
	, OrthoNearClipPlane(0.0f)
	, OrthoFarClipPlane(-1.0f)
{

}

// Return true if the component itself has been modified
bool 
UHoudiniInputSplineComponent::HasComponentChanged() const 
{
	USplineComponent* SplineComponent = Cast<USplineComponent>(InputObject.LoadSynchronous());

	if (!SplineComponent)
		return false;

	if (SplineClosed != SplineComponent->IsClosedLoop()) 
		return true;


	if (SplineComponent->GetNumberOfSplinePoints() != NumberOfSplineControlPoints)
		return true;

	for (int32 n = 0; n < SplineComponent->GetNumberOfSplinePoints(); ++n) 
	{
		const FTransform &CurSplineComponentTransform = SplineComponent->GetTransformAtSplinePoint(n, ESplineCoordinateSpace::Local);
		const FTransform &CurInputTransform = SplineControlPoints[n];

		if (CurInputTransform.GetLocation() != CurSplineComponentTransform.GetLocation())
			return true;

		if (CurInputTransform.GetRotation().Rotator() != CurSplineComponentTransform.GetRotation().Rotator())
			return true;

		if (CurInputTransform.GetScale3D() != CurSplineComponentTransform.GetScale3D())
			return true;
	}

	return false;
}

//
UHoudiniInputHoudiniSplineComponent::UHoudiniInputHoudiniSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurveType(EHoudiniCurveType::Polygon)
	, CurveMethod(EHoudiniCurveMethod::CVs)
	, Reversed(false)
{

}

//
UHoudiniInputHoudiniAsset::UHoudiniInputHoudiniAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AssetOutputIndex(-1)
{

}

//
UHoudiniInputActor::UHoudiniInputActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastUpdateNumComponentsAdded(0)
	, LastUpdateNumComponentsRemoved(0)
{

}

//
UHoudiniInputLandscape::UHoudiniInputLandscape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

//
UHoudiniInputBrush::UHoudiniInputBrush()
	: CombinedModel(nullptr)
	, bIgnoreInputObject(false)
{

}

//-----------------------------------------------------------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------------------------------------------------------

UObject* 
UHoudiniInputObject::GetObject() const
{
	return InputObject.LoadSynchronous();
}

UStaticMesh*
UHoudiniInputStaticMesh::GetStaticMesh() const
{
	return Cast<UStaticMesh>(InputObject.LoadSynchronous());
}

UBlueprint* 
UHoudiniInputStaticMesh::GetBlueprint() const 
{
	return Cast<UBlueprint>(InputObject.LoadSynchronous());
}

bool UHoudiniInputStaticMesh::bIsBlueprint() const 
{
	return (InputObject.IsValid() && InputObject.Get()->IsA<UBlueprint>());
}

USkeletalMesh*
UHoudiniInputSkeletalMesh::GetSkeletalMesh()
{
	return Cast<USkeletalMesh>(InputObject.LoadSynchronous());
}

UGeometryCollection*
UHoudiniInputGeometryCollection::GetGeometryCollection()
{
	return Cast<UGeometryCollection>(InputObject.LoadSynchronous());
}

UGeometryCollectionComponent*
UHoudiniInputGeometryCollectionComponent::GetGeometryCollectionComponent()
{
	return Cast<UGeometryCollectionComponent>(InputObject.LoadSynchronous());
}

UGeometryCollection* UHoudiniInputGeometryCollectionComponent::GetGeometryCollection()
{
	UGeometryCollectionComponent * GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (!IsValid(GeometryCollectionComponent))
	{
		return nullptr;
	}
	
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
	return GeometryCollectionEdit.GetRestCollection();
}

AGeometryCollectionActor*
UHoudiniInputGeometryCollectionActor::GetGeometryCollectionActor()
{
	return Cast<AGeometryCollectionActor>(InputObject.LoadSynchronous());
}

UGeometryCollectionComponent* UHoudiniInputGeometryCollectionActor::GetGeometryCollectionComponent()
{
	AGeometryCollectionActor * GeometryCollectionActor = GetGeometryCollectionActor();
	if (!IsValid(GeometryCollectionActor))
	{
		return nullptr;
	}

	return GeometryCollectionActor->GetGeometryCollectionComponent();
}

UGeometryCollection* UHoudiniInputGeometryCollectionActor::GetGeometryCollection()
{
	UGeometryCollectionComponent * GeometryCollectionComponent = GetGeometryCollectionComponent();
	if (!IsValid(GeometryCollectionComponent))
	{
		return nullptr;
	}
	
	FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
	return GeometryCollectionEdit.GetRestCollection();
}

USceneComponent*
UHoudiniInputSceneComponent::GetSceneComponent()
{
	return Cast<USceneComponent>(InputObject.LoadSynchronous());
}

UStaticMeshComponent*
UHoudiniInputMeshComponent::GetStaticMeshComponent()
{
	return Cast<UStaticMeshComponent>(InputObject.LoadSynchronous());
}

UStaticMesh*
UHoudiniInputMeshComponent::GetStaticMesh() 
{ 
	return StaticMesh.Get(); 
}

UInstancedStaticMeshComponent*
UHoudiniInputInstancedMeshComponent::GetInstancedStaticMeshComponent() 
{
	return Cast<UInstancedStaticMeshComponent>(InputObject.LoadSynchronous());
}

USplineComponent*
UHoudiniInputSplineComponent::GetSplineComponent()
{
	return Cast<USplineComponent>(InputObject.LoadSynchronous());
}

UHoudiniSplineComponent*
UHoudiniInputHoudiniSplineComponent::GetCurveComponent() const
{
	return Cast<UHoudiniSplineComponent>(GetObject());
	//return Cast<UHoudiniSplineComponent>(InputObject.LoadSynchronous());
}

UCameraComponent*
UHoudiniInputCameraComponent::GetCameraComponent()
{
	return Cast<UCameraComponent>(InputObject.LoadSynchronous());
}

UHoudiniAssetComponent*
UHoudiniInputHoudiniAsset::GetHoudiniAssetComponent()
{
	return Cast<UHoudiniAssetComponent>(InputObject.LoadSynchronous());
}

AActor*
UHoudiniInputActor::GetActor() const
{
	return Cast<AActor>(InputObject.LoadSynchronous());
}

ALandscapeProxy*
UHoudiniInputLandscape::GetLandscapeProxy() const
{
	return Cast<ALandscapeProxy>(InputObject.LoadSynchronous());
}

void 
UHoudiniInputLandscape::SetLandscapeProxy(UObject* InLandscapeProxy) 
{
	UObject* LandscapeProxy = Cast<UObject>(InLandscapeProxy);
	if (LandscapeProxy)
		InputObject = LandscapeProxy;
}

int32 UHoudiniInputLandscape::CountLandscapeComponents() const
{
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (!IsValid(LandscapeProxy))
	{
		return false;
	}
	
	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!IsValid(LandscapeInfo))
	{
		return false; 
	}

	int32 NumComponents = 0;
	LandscapeInfo->ForAllLandscapeProxies([&NumComponents](ALandscapeProxy* Proxy)
	{
		if (IsValid(Proxy))
		{
			NumComponents += Proxy->LandscapeComponents.Num();
		}
	});
	return NumComponents;
}

ABrush*
UHoudiniInputBrush::GetBrush() const
{ 
	return Cast<ABrush>(InputObject.LoadSynchronous());
}


//-----------------------------------------------------------------------------------------------------------------------------
// CREATE METHODS
//-----------------------------------------------------------------------------------------------------------------------------

UHoudiniInputObject *
UHoudiniInputObject::CreateTypedInputObject(UObject * InObject, UObject* InOuter, const FString& InName)
{
	if (!InObject)
		return nullptr;

	UHoudiniInputObject* HoudiniInputObject = nullptr;
	
	EHoudiniInputObjectType InputObjectType = GetInputObjectTypeFromObject(InObject);
	switch (InputObjectType)
	{
		case EHoudiniInputObjectType::Object:
			HoudiniInputObject = UHoudiniInputObject::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::StaticMesh:
			HoudiniInputObject = UHoudiniInputStaticMesh::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::SkeletalMesh:
			HoudiniInputObject = UHoudiniInputSkeletalMesh::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::SceneComponent:
			// Do not create input objects for unknown scene component!
			//HoudiniInputObject = UHoudiniInputSceneComponent::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::StaticMeshComponent:
			HoudiniInputObject = UHoudiniInputMeshComponent::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::InstancedStaticMeshComponent:
			HoudiniInputObject = UHoudiniInputInstancedMeshComponent::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::SplineComponent:
			HoudiniInputObject = UHoudiniInputSplineComponent::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::HoudiniSplineComponent:
			HoudiniInputObject = UHoudiniInputHoudiniSplineComponent::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::HoudiniAssetActor:
			{
				AHoudiniAssetActor* HoudiniActor = Cast<AHoudiniAssetActor>(InObject);
				if (HoudiniActor)
				{
					HoudiniInputObject = UHoudiniInputHoudiniAsset::Create(HoudiniActor->GetHoudiniAssetComponent(), InOuter, InName);
				}
				else
				{
					HoudiniInputObject = nullptr;
				}
			}
			break;

		case EHoudiniInputObjectType::HoudiniAssetComponent:
			HoudiniInputObject = UHoudiniInputHoudiniAsset::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::Actor:
			HoudiniInputObject = UHoudiniInputActor::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::Landscape:
			HoudiniInputObject = UHoudiniInputLandscape::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::Brush:
			HoudiniInputObject = UHoudiniInputBrush::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::CameraComponent:
			HoudiniInputObject = UHoudiniInputCameraComponent::Create(InObject, InOuter, InName);
			break;

		case EHoudiniInputObjectType::DataTable:
			HoudiniInputObject = UHoudiniInputDataTable::Create(InObject, InOuter, InName);
			break;
		
		case EHoudiniInputObjectType::FoliageType_InstancedStaticMesh:
			HoudiniInputObject = UHoudiniInputFoliageType_InstancedStaticMesh::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::GeometryCollection:
			HoudiniInputObject = UHoudiniInputGeometryCollection::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::GeometryCollectionComponent:
			HoudiniInputObject = UHoudiniInputGeometryCollectionComponent::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::GeometryCollectionActor:
			HoudiniInputObject = UHoudiniInputGeometryCollectionActor::Create(InObject, InOuter, InName);
			break;
		case EHoudiniInputObjectType::Invalid:
		default:		
			break;
	}

	return HoudiniInputObject;
}


UHoudiniInputObject *
UHoudiniInputInstancedMeshComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_ISMC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputInstancedMeshComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputInstancedMeshComponent * HoudiniInputObject = NewObject<UHoudiniInputInstancedMeshComponent>(
		InOuter, UHoudiniInputInstancedMeshComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::InstancedStaticMeshComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputMeshComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_SMC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputMeshComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputMeshComponent * HoudiniInputObject = NewObject<UHoudiniInputMeshComponent>(
		InOuter, UHoudiniInputMeshComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::StaticMeshComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputSplineComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_Spline_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSplineComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSplineComponent * HoudiniInputObject = NewObject<UHoudiniInputSplineComponent>(
		InOuter, UHoudiniInputSplineComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::SplineComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputHoudiniSplineComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_HoudiniSpline_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputHoudiniSplineComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputHoudiniSplineComponent * HoudiniInputObject = NewObject<UHoudiniInputHoudiniSplineComponent>(
		InOuter, UHoudiniInputHoudiniSplineComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::HoudiniSplineComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputCameraComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_Camera_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputCameraComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputCameraComponent * HoudiniInputObject = NewObject<UHoudiniInputCameraComponent>(
		InOuter, UHoudiniInputCameraComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::CameraComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputHoudiniAsset::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	UHoudiniAssetComponent * InHoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InObject);
	if (!InHoudiniAssetComponent)
		return nullptr;

	FString InputObjectNameStr = "HoudiniInputObject_HAC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputHoudiniAsset::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputHoudiniAsset * HoudiniInputObject = NewObject<UHoudiniInputHoudiniAsset>(
		InOuter, UHoudiniInputHoudiniAsset::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::HoudiniAssetComponent;

	HoudiniInputObject->InputNodeId = InHoudiniAssetComponent->GetAssetId();
	HoudiniInputObject->InputObjectNodeId = InHoudiniAssetComponent->GetAssetId();

	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputSceneComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_SceneComp_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSceneComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSceneComponent * HoudiniInputObject = NewObject<UHoudiniInputSceneComponent>(
		InOuter, UHoudiniInputSceneComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::SceneComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputLandscape::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_Landscape_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputLandscape::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputLandscape * HoudiniInputObject = NewObject<UHoudiniInputLandscape>(
		InOuter, UHoudiniInputLandscape::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Landscape;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputBrush *
UHoudiniInputBrush::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_Brush_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputBrush::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputBrush * HoudiniInputObject = NewObject<UHoudiniInputBrush>(
		InOuter, UHoudiniInputBrush::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Brush;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputActor::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_Actor_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputActor::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputActor * HoudiniInputObject = NewObject<UHoudiniInputActor>(
		InOuter, UHoudiniInputActor::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Actor;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputStaticMesh::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_SM_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputStaticMesh::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputStaticMesh * HoudiniInputObject = NewObject<UHoudiniInputStaticMesh>(
		InOuter, UHoudiniInputStaticMesh::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::StaticMesh;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

// void UHoudiniInputStaticMesh::DuplicateAndCopyState(UObject* DestOuter, UHoudiniInputStaticMesh*& OutNewInput)
// {
// 	UHoudiniInputStaticMesh* NewInput = Cast<UHoudiniInputStaticMesh>(StaticDuplicateObject(this, DestOuter));
// 	OutNewInput = NewInput;
// 	OutNewInput->CopyStateFrom(this, false);
// }

void
UHoudiniInputStaticMesh::CopyStateFrom(UHoudiniInputObject* InInput, bool bCopyAllProperties)
{
	UHoudiniInputStaticMesh* StaticMeshInput = Cast<UHoudiniInputStaticMesh>(InInput); 
	check(InInput);

	TArray<UHoudiniInputStaticMesh*> PrevInputs = BlueprintStaticMeshes;
	
	Super::CopyStateFrom(StaticMeshInput, bCopyAllProperties);

	const int32 NumInputs = StaticMeshInput->BlueprintStaticMeshes.Num();
	BlueprintStaticMeshes = PrevInputs;
	TArray<UHoudiniInputStaticMesh*> StaleInputs(BlueprintStaticMeshes);

	BlueprintStaticMeshes.SetNum(NumInputs);

	for (int i = 0; i < NumInputs; ++i)
	{
		UHoudiniInputStaticMesh* FromInput = StaticMeshInput->BlueprintStaticMeshes[i];
		UHoudiniInputStaticMesh* ToInput = BlueprintStaticMeshes[i];

		if (!FromInput)
		{
			BlueprintStaticMeshes[i] = nullptr;
			continue;
		}

		if (ToInput)
		{
			// Check whether the ToInput can be reused
			bool bIsValid = true;
			bIsValid = bIsValid && ToInput->Matches(*FromInput);
			bIsValid = bIsValid && ToInput->GetOuter() == this;
			if (!bIsValid)
			{
				ToInput = nullptr;
			}
		}

		if (ToInput)
		{
			// We have a reusable input
			ToInput->CopyStateFrom(FromInput, true);
		}
		else
		{
			// We need to create a new input
			ToInput = Cast<UHoudiniInputStaticMesh>(FromInput->DuplicateAndCopyState(this));
		}

		BlueprintStaticMeshes[i] = ToInput;
	}

	for(UHoudiniInputStaticMesh* StaleInput : StaleInputs)
	{
		if (!StaleInput)
			continue;
		StaleInput->InvalidateData();
	}
}

void
UHoudiniInputStaticMesh::SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes)
{
	Super::SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	for(UHoudiniInputStaticMesh* Input : BlueprintStaticMeshes)
	{
		if (!Input)
			continue;
		Input->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}
}

void
UHoudiniInputStaticMesh::InvalidateData()
{
	for(UHoudiniInputStaticMesh* Input : BlueprintStaticMeshes)
	{
		if (!Input)
			continue;
		Input->InvalidateData();
	}

	Super::InvalidateData();
}


UHoudiniInputObject *
UHoudiniInputSkeletalMesh::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_SkelMesh_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputSkeletalMesh::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputSkeletalMesh * HoudiniInputObject = NewObject<UHoudiniInputSkeletalMesh>(
		InOuter, UHoudiniInputSkeletalMesh::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::SkeletalMesh;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputGeometryCollection::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_GC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputGeometryCollection::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputGeometryCollection * HoudiniInputObject = NewObject<UHoudiniInputGeometryCollection>(
                InOuter, UHoudiniInputGeometryCollection::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::GeometryCollection;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputGeometryCollectionComponent::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_GCC_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputGeometryCollectionComponent::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputGeometryCollectionComponent * HoudiniInputObject = NewObject<UHoudiniInputGeometryCollectionComponent>(
                InOuter, UHoudiniInputGeometryCollectionComponent::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::GeometryCollectionComponent;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputGeometryCollectionActor::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_GCA_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputGeometryCollectionActor::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputGeometryCollectionActor * HoudiniInputObject = NewObject<UHoudiniInputGeometryCollectionActor>(
                InOuter, UHoudiniInputGeometryCollectionActor::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::GeometryCollectionActor;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UHoudiniInputObject *
UHoudiniInputObject::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputObject::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputObject * HoudiniInputObject = NewObject<UHoudiniInputObject>(
		InOuter, UHoudiniInputObject::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::Object;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

bool
UHoudiniInputObject::Matches(const UHoudiniInputObject& Other) const
{
	return (Type == Other.Type 
		&& InputNodeId == Other.InputNodeId
		&& InputObjectNodeId == Other.InputObjectNodeId
		);
}

//-----------------------------------------------------------------------------------------------------------------------------
// DELETE METHODS
//-----------------------------------------------------------------------------------------------------------------------------

void
UHoudiniInputObject::InvalidateData()
{
	// If valid, mark our input nodes for deletion..	
	if (this->IsA<UHoudiniInputHoudiniAsset>() || !bCanDeleteHoudiniNodes)
	{
		// Unless if we're a HoudiniAssetInput! we don't want to delete the other HDA's node!
		// just invalidate the node IDs!
		InputNodeId = -1;
		InputObjectNodeId = -1;
		return;
	}

	if (InputNodeId >= 0)
	{
		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputNodeId);
		InputNodeId = -1;
	}

	// ... and the parent OBJ as well to clean up
	if (InputObjectNodeId >= 0)
	{
		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputObjectNodeId);
		InputObjectNodeId = -1;
	}

	
}

void
UHoudiniInputObject::BeginDestroy()
{
	// Invalidate and mark our input node for deletion
	InvalidateData();

	Super::BeginDestroy();
}

//-----------------------------------------------------------------------------------------------------------------------------
// UPDATE METHODS
//-----------------------------------------------------------------------------------------------------------------------------

void
UHoudiniInputObject::Update(UObject * InObject)
{
	InputObject = InObject;
}

void
UHoudiniInputStaticMesh::Update(UObject * InObject)
{
	// Nothing to do
	Super::Update(InObject);
	// Static Mesh input accepts SM, BP, FoliageType_InstancedStaticMesh (static mesh) and FoliageType_Actor (if blueprint actor).
	UStaticMesh* SM = Cast<UStaticMesh>(InObject);
	UBlueprint* BP = Cast<UBlueprint>(InObject);

	ensure(SM || BP);
}

void
UHoudiniInputSkeletalMesh::Update(UObject * InObject)
{
	// Nothing to do
	Super::Update(InObject);

	USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(InObject);
	ensure(SkelMesh);
}

void
UHoudiniInputGeometryCollection::Update(UObject * InObject)
{
	// Nothing to do
	Super::Update(InObject);

	UGeometryCollection* GeometryCollection = Cast<UGeometryCollection>(InObject);
	ensure(GeometryCollection);
}


void
UHoudiniInputGeometryCollectionComponent::Update(UObject * InObject)
{
	// Nothing to do
	Super::Update(InObject);

	UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(InObject);
	ensure(GeometryCollectionComponent);
}


void
UHoudiniInputGeometryCollectionActor::Update(UObject * InObject)
{
	// Nothing to do
	Super::Update(InObject);

	AGeometryCollectionActor* GeometryCollectionActor = Cast<AGeometryCollectionActor>(InObject);
	ensure(GeometryCollectionActor);
}

void
UHoudiniInputSceneComponent::Update(UObject * InObject)
{	
	Super::Update(InObject);

	USceneComponent* USC = Cast<USceneComponent>(InObject);	
	ensure(USC);
	if (USC)
	{
		Transform = USC->GetComponentTransform();
	}
}


bool 
UHoudiniInputSceneComponent::HasActorTransformChanged() const
{
	// Returns true if the attached actor's (parent) transform has been modified
	USceneComponent* MyComp = Cast<USceneComponent>(InputObject.LoadSynchronous());
	if (!IsValid(MyComp))
		return false;

	AActor* MyActor = MyComp->GetOwner();
	if (!MyActor)
		return false;

	return (!ActorTransform.Equals(MyActor->GetTransform()));
}


bool
UHoudiniInputSceneComponent::HasComponentTransformChanged() const
{
	// Returns true if the attached actor's (parent) transform has been modified
	USceneComponent* MyComp = Cast<USceneComponent>(InputObject.LoadSynchronous());
	if (!IsValid(MyComp))
		return false;

	return !Transform.Equals(MyComp->GetComponentTransform());
}


bool 
UHoudiniInputSceneComponent::HasComponentChanged() const
{
	// Should return true if the component itself has been modified
	// Should be overriden in child classes
	return false;
}


bool
UHoudiniInputMeshComponent::HasComponentChanged() const
{
	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InputObject.LoadSynchronous());
	UStaticMesh* MySM = StaticMesh.Get();

	// Return true if SMC's static mesh has been modified
	return (MySM != SMC->GetStaticMesh());
}

bool
UHoudiniInputCameraComponent::HasComponentChanged() const
{
	UCameraComponent* Camera = Cast<UCameraComponent>(InputObject.LoadSynchronous());	
	if (IsValid(Camera))
	{
		bool bOrtho = Camera->ProjectionMode == ECameraProjectionMode::Type::Orthographic;
		if (bOrtho != bIsOrthographic)
			return true;

		if (Camera->FieldOfView != FOV)
			return true;

		if (Camera->AspectRatio != AspectRatio)
			return true;

		if (Camera->OrthoWidth != OrthoWidth)
			return true;

		if (Camera->OrthoNearClipPlane != OrthoNearClipPlane)
			return true;

		if (Camera->OrthoFarClipPlane != OrthoFarClipPlane)
			return true;
	}
		
	return false;
}



void
UHoudiniInputCameraComponent::Update(UObject * InObject)
{
	Super::Update(InObject);

	UCameraComponent* Camera = Cast<UCameraComponent>(InputObject.LoadSynchronous());

	ensure(Camera);
	
	if (IsValid(Camera))
	{	
		bIsOrthographic = Camera->ProjectionMode == ECameraProjectionMode::Type::Orthographic;
		FOV = Camera->FieldOfView;
		AspectRatio = Camera->AspectRatio;
		OrthoWidth = Camera->OrthoWidth;
		OrthoNearClipPlane = Camera->OrthoNearClipPlane;
		OrthoFarClipPlane = Camera->OrthoFarClipPlane;
	}
}

void
UHoudiniInputMeshComponent::Update(UObject * InObject)
{
	Super::Update(InObject);

	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(InObject);
	
	ensure(SMC);

	if (SMC)
	{
		StaticMesh = TSoftObjectPtr<UStaticMesh>(SMC->GetStaticMesh());

		TArray<UMaterialInterface*> Materials = SMC->GetMaterials();
		for (auto CurrentMat : Materials)
		{
			// TODO: Update material ref here
			FString MatRef;
			MeshComponentsMaterials.Add(MatRef);
		}
	}
}

void
UHoudiniInputInstancedMeshComponent::Update(UObject * InObject)
{
	Super::Update(InObject);

	UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InObject);

	ensure(ISMC);

	if (ISMC)
	{
		uint32 InstanceCount = ISMC->GetInstanceCount();
		InstanceTransforms.SetNum(InstanceCount);

		// Copy the instances' transforms		
		for (uint32 InstIdx = 0; InstIdx < InstanceCount; InstIdx++)
		{
			FTransform CurTransform = FTransform::Identity;
			ISMC->GetInstanceTransform(InstIdx, CurTransform);
			InstanceTransforms[InstIdx] = CurTransform;			
		}
	}
}

bool
UHoudiniInputInstancedMeshComponent::HasInstancesChanged() const
{	
	UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(InputObject.LoadSynchronous());
	if (!ISMC)
		return false;

	uint32 InstanceCount = ISMC->GetInstanceCount();
	if (InstanceTransforms.Num() != InstanceCount)
		return true;

	// Copy the instances' transforms		
	for (uint32 InstIdx = 0; InstIdx < InstanceCount; InstIdx++)
	{
		FTransform CurTransform = FTransform::Identity;
		ISMC->GetInstanceTransform(InstIdx, CurTransform);

		if(!InstanceTransforms[InstIdx].Equals(CurTransform))
			return true;
	}

	return false;
}

bool
UHoudiniInputInstancedMeshComponent::HasComponentTransformChanged() const
{
	if (Super::HasComponentTransformChanged())
		return true;

	return HasInstancesChanged();
}

void
UHoudiniInputSplineComponent::Update(UObject * InObject)
{
	Super::Update(InObject);

	USplineComponent* Spline = Cast<USplineComponent>(InObject);

	ensure(Spline);

	if (Spline)
	{
		NumberOfSplineControlPoints = Spline->GetNumberOfSplinePoints();
		SplineLength = Spline->GetSplineLength();		
		SplineClosed = Spline->IsClosedLoop();

		//SplineResolution = -1.0f;

		SplineControlPoints.SetNumZeroed(NumberOfSplineControlPoints);
		for (int32 Idx = 0; Idx < NumberOfSplineControlPoints; Idx++)
		{
			SplineControlPoints[Idx] = Spline->GetTransformAtSplinePoint(Idx, ESplineCoordinateSpace::Local);
		}		
	}
}

void
UHoudiniInputHoudiniSplineComponent::Update(UObject* InObject)
{
	Super::Update(InObject);

	// We store the component references as a normal pointer property instead of using a soft object reference.
	// If we use a soft object reference, the editor will complain about deleting a reference that is in use 
	// everytime we try to delete the actor, even though everything is contained within the actor.

	CachedComponent = Cast<UHoudiniSplineComponent>(InObject);
	InputObject = nullptr;

	// We need a strong ref to the spline component to prevent it from being GCed
	//MyHoudiniSplineComponent = Cast<UHoudiniSplineComponent>(InObject);
	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();

	if (!IsValid(HoudiniSplineComponent))
	{
		// Use default values
		CurveType = EHoudiniCurveType::Polygon;
		CurveMethod = EHoudiniCurveMethod::CVs;
		Reversed = false;
	}
	else
	{
		CurveType = HoudiniSplineComponent->GetCurveType();
		CurveMethod = HoudiniSplineComponent->GetCurveMethod();
		Reversed = false;//Spline->IsReversed();
	}
}

UObject*
UHoudiniInputHoudiniSplineComponent::GetObject() const
{
	return CachedComponent;
}

void
UHoudiniInputHoudiniSplineComponent::MarkChanged(const bool& bInChanged)
{
	Super::MarkChanged(bInChanged);
	
	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent)
	{
		HoudiniSplineComponent->MarkChanged(bInChanged);
	}
}

void
UHoudiniInputHoudiniSplineComponent::SetNeedsToTriggerUpdate(const bool& bInTriggersUpdate)
{
	Super::SetNeedsToTriggerUpdate(bInTriggersUpdate);

	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent)
	{
		HoudiniSplineComponent->SetNeedsToTriggerUpdate(bInTriggersUpdate);
	}
}

bool
UHoudiniInputHoudiniSplineComponent::HasChanged() const
{
	if (Super::HasChanged())
		return true;

	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent && HoudiniSplineComponent->HasChanged())
		return true;

	return false;
}

bool
UHoudiniInputHoudiniSplineComponent::NeedsToTriggerUpdate() const
{
	if (Super::NeedsToTriggerUpdate())
		return true;
	UHoudiniSplineComponent* HoudiniSplineComponent = GetCurveComponent();
	if (HoudiniSplineComponent && HoudiniSplineComponent->NeedsToTriggerUpdate())
		return true;

	return false;
}

void
UHoudiniInputHoudiniAsset::Update(UObject * InObject)
{
	Super::Update(InObject);

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InObject);

	ensure(HAC);

	if (HAC)
	{
		// TODO: Notify HAC that we're a downstream?
		InputNodeId = HAC->GetAssetId();
		InputObjectNodeId = HAC->GetAssetId();

		// TODO: Allow selection of the asset output
		AssetOutputIndex = 0;
	}
}


void
UHoudiniInputActor::Update(UObject * InObject)
{
	const bool bHasInputObjectChanged = InputObject != InObject;
	
	Super::Update(InObject);

	AActor* Actor = Cast<AActor>(InObject);
	ensure(Actor);

	if (Actor)
	{
		Transform = Actor->GetTransform();

		// If we are updating (InObject == InputObject), then remove stale components and add new components,
		// if InObject != InputObject, remove all components and rebuild

		if (bHasInputObjectChanged)
		{
			// The actor's components that can be sent as inputs
			LastUpdateNumComponentsRemoved = ActorComponents.Num();

			ActorComponents.Empty();
			ActorSceneComponents.Empty();

			TArray<USceneComponent*> AllComponents;
			Actor->GetComponents<USceneComponent>(AllComponents, true);
			
			ActorComponents.Reserve(AllComponents.Num());
			for (USceneComponent * SceneComponent : AllComponents)
			{
				if (!IsValid(SceneComponent))
					continue;
				if (!ShouldTrackComponent(SceneComponent))
					continue;

				UHoudiniInputObject* InputObj = UHoudiniInputObject::CreateTypedInputObject(
					SceneComponent, GetOuter(), Actor->GetName());
				if (!InputObj)
					continue;

				UHoudiniInputSceneComponent* SceneInput = Cast<UHoudiniInputSceneComponent>(InputObj);
				if (!SceneInput)
					continue;

				ActorComponents.Add(SceneInput);
				ActorSceneComponents.Add(TSoftObjectPtr<UObject>(SceneComponent));
			}
			LastUpdateNumComponentsAdded = ActorComponents.Num();
		}
		else
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = 0;

			// Look for any components to add or remove
			TSet<USceneComponent*> NewComponents;
			const bool bIncludeFromChildActors = true;
			Actor->ForEachComponent<USceneComponent>(bIncludeFromChildActors, [&](USceneComponent* InComp)
			{
				if (IsValid(InComp))
				{
					if (!ActorSceneComponents.Contains(InComp) && ShouldTrackComponent(InComp))
					{
						NewComponents.Add(InComp);
					}
				}
			});
			
			// Update the actor input components (from the same actor)
			TArray<int32> ComponentIndicesToRemove;
			const int32 NumActorComponents = ActorComponents.Num(); 
			for (int32 Index = 0; Index < NumActorComponents; ++Index)
			{
				UHoudiniInputSceneComponent* CurActorComp = ActorComponents[Index];
				if (!IsValid(CurActorComp))
				{
					ComponentIndicesToRemove.Add(Index);
					continue;
				}

				// Does the component still exist on Actor?
				UObject* const CompObj = CurActorComp->GetObject();
				// Make sure the actor is still valid
				if (!IsValid(CompObj))
				{
					// If it's not, mark it for deletion
					if ((CurActorComp->InputNodeId > 0) || (CurActorComp->InputObjectNodeId > 0))
					{
						CurActorComp->InvalidateData();
					}

					ComponentIndicesToRemove.Add(Index);
					continue;
				}
			}

			// Remove the destroyed/invalid components
			const int32 NumToRemove = ComponentIndicesToRemove.Num();
			if (NumToRemove > 0)
			{
				for (int32 Index = NumToRemove - 1; Index >= 0; --Index)
				{
					const int32& IndexToRemove = ComponentIndicesToRemove[Index];
					
					UHoudiniInputSceneComponent* const CurActorComp = ActorComponents[IndexToRemove];
					if (CurActorComp)
						ActorSceneComponents.Remove(CurActorComp->InputObject);

					const bool bAllowShrink = false;
					ActorComponents.RemoveAtSwap(IndexToRemove, 1, bAllowShrink);

					LastUpdateNumComponentsRemoved++;
				}
			}

			if (NewComponents.Num() > 0)
			{
				for (USceneComponent * SceneComponent : NewComponents)
				{
					if (!IsValid(SceneComponent))
						continue;

					UHoudiniInputObject* InputObj = UHoudiniInputObject::CreateTypedInputObject(
						SceneComponent, GetOuter(), Actor->GetName());
					if (!InputObj)
						continue;

					UHoudiniInputSceneComponent* SceneInput = Cast<UHoudiniInputSceneComponent>(InputObj);
					if (!SceneInput)
						continue;

					ActorComponents.Add(SceneInput);
					ActorSceneComponents.Add(SceneComponent);

					LastUpdateNumComponentsAdded++;
				}
			}

			if (LastUpdateNumComponentsAdded > 0 || LastUpdateNumComponentsRemoved > 0)
			{
				ActorComponents.Shrink();
			}
		}
	}
	else
	{
		// If we don't have a valid actor or null, delete any input components we still have and mark as changed
		if (ActorComponents.Num() > 0)
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = ActorComponents.Num();
			ActorComponents.Empty();
			ActorSceneComponents.Empty();
		}
		else
		{
			LastUpdateNumComponentsAdded = 0;
			LastUpdateNumComponentsRemoved = 0;
		}
	}
}

bool 
UHoudiniInputActor::HasActorTransformChanged() const
{
	if (!GetActor())
		return false;

	if (HasRootComponentTransformChanged())
		return true;

	if (HasComponentsTransformChanged())
		return true;

	return false;
}

bool UHoudiniInputActor::HasRootComponentTransformChanged() const
{
	if (!Transform.Equals(GetActor()->GetTransform()))
		return true;
	return false;
}

bool UHoudiniInputActor::HasComponentsTransformChanged() const
{
	// Search through all the child components to see if we have changed
	// transforms in there.
	for (auto CurActorComp : GetActorComponents())
	{
		if (CurActorComp->HasComponentTransformChanged())
		{
			return true;
		}
	}

	return false;
}

bool 
UHoudiniInputActor::HasContentChanged() const
{
	return false;
}

bool
UHoudiniInputActor::GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects)
{
	if (Super::GetChangedObjectsAndValidNodes(OutChangedObjects, OutNodeIdsOfUnchangedValidObjects))
		return true;

	bool bAnyChanges = false;
	// Check each of its child objects (components)
	for (auto* const CurrentComp : GetActorComponents())
	{
		if (!IsValid(CurrentComp))
			continue;

		if (CurrentComp->GetChangedObjectsAndValidNodes(OutChangedObjects, OutNodeIdsOfUnchangedValidObjects))
			bAnyChanges = true;
	}

	return bAnyChanges;
}

void
UHoudiniInputActor::InvalidateData()
{
	// Call invalidate on input component objects
	for (auto* const CurrentComp : GetActorComponents())
	{
		if (!IsValid(CurrentComp))
			continue;
		
		CurrentComp->InvalidateData();
	}
	
	Super::InvalidateData();
}

bool UHoudiniInputLandscape::ShouldTrackComponent(UActorComponent* InComponent)
{
	// We only track LandscapeComponents for landscape inputs since the Landscape tools
	// have this very interesting and creative way of adding components when the tool is activated
	// (looking at you Flatten tool) which causes cooking loops.
	if (!IsValid(InComponent))
		return false;
	return InComponent->IsA(ULandscapeComponent::StaticClass());
}

bool UHoudiniInputLandscape::HasContentChanged() const
{
	if (Super::HasContentChanged())
	{
		return true;
	}

	const int32 NumComponents = CountLandscapeComponents();
	return NumComponents != CachedNumLandscapeComponents;
}

void
UHoudiniInputLandscape::Update(UObject * InObject)
{
	Super::Update(InObject);

	const ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(InObject);

	//ensure(Landscape);

	if (Landscape)
	{
		Transform = FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(Landscape->GetLandscapeInfo());
		CachedNumLandscapeComponents = CountLandscapeComponents();
	}
}

bool UHoudiniInputLandscape::HasActorTransformChanged() const
{
	if (!GetActor())
		return false;

	if (HasComponentsTransformChanged())
		return true;

	// We replace the root component transform comparison, with a transform that is calculated, for Houdini, based
	// on the currently loaded tiles.
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (IsValid(LandscapeProxy))
	{
		const FTransform HoudiniTransform = FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(LandscapeProxy->GetLandscapeInfo());
		if (!Transform.Equals(HoudiniTransform))
		{
			return true;
		}
	}

	return false;
}

EHoudiniInputObjectType
UHoudiniInputObject::GetInputObjectTypeFromObject(UObject* InObject)
{
	if (InObject->IsA(USceneComponent::StaticClass()))
	{
		// Handle component inputs
		// UISMC derived from USMC, so always test instances before static meshes
		if (InObject->IsA(UInstancedStaticMeshComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::InstancedStaticMeshComponent;
		}
		else if (InObject->IsA(UStaticMeshComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::StaticMeshComponent;
		}
		else if (InObject->IsA(USplineComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::SplineComponent;
		}
		else if (InObject->IsA(UHoudiniSplineComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::HoudiniSplineComponent;
		}
		else if (InObject->IsA(UHoudiniAssetComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::HoudiniAssetComponent;
		}
		else if (InObject->IsA(UCameraComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::CameraComponent;
		}
		else if (InObject->IsA(UGeometryCollectionComponent::StaticClass()))
		{
			return EHoudiniInputObjectType::GeometryCollectionComponent;
		}
		else
		{
			return EHoudiniInputObjectType::SceneComponent;
		}
	}
	else if (InObject->IsA(AActor::StaticClass()))
	{
		// Handle actors
		if (InObject->IsA(ALandscapeProxy::StaticClass()))
		{
			return EHoudiniInputObjectType::Landscape;
		}
		else if (InObject->IsA(ABrush::StaticClass()))
		{
			return EHoudiniInputObjectType::Brush;
		}
		else if (InObject->IsA(AHoudiniAssetActor::StaticClass()))
		{
			return EHoudiniInputObjectType::HoudiniAssetActor;
		}
		else if (InObject->IsA(AGeometryCollectionActor::StaticClass()))
		{
			return EHoudiniInputObjectType::GeometryCollectionActor;
		}
		else
		{
			return EHoudiniInputObjectType::Actor;
		}
	}
	else if (InObject->IsA(UBlueprint::StaticClass())) 
	{
		return EHoudiniInputObjectType::StaticMesh;
	}
	else if (InObject->IsA(UFoliageType_InstancedStaticMesh::StaticClass())) 
	{
		return EHoudiniInputObjectType::FoliageType_InstancedStaticMesh;
	}
	else
	{
		if (InObject->IsA(UStaticMesh::StaticClass()))
		{
			return EHoudiniInputObjectType::StaticMesh;
		}
		else if (InObject->IsA(USkeletalMesh::StaticClass()))
		{
			return EHoudiniInputObjectType::SkeletalMesh;
		}
		else if (InObject->IsA(UGeometryCollection::StaticClass()))
		{
			return EHoudiniInputObjectType::GeometryCollection;
		}
		else if (InObject->IsA(UDataTable::StaticClass()))
		{
			return EHoudiniInputObjectType::DataTable;
		}
		else
		{
			return EHoudiniInputObjectType::Object;
		}
	}

	return EHoudiniInputObjectType::Invalid;
}



//-----------------------------------------------------------------------------------------------------------------------------
// UHoudiniInputBrush
//-----------------------------------------------------------------------------------------------------------------------------

FHoudiniBrushInfo::FHoudiniBrushInfo()
	: CachedTransform()
	, CachedOrigin(ForceInitToZero)
	, CachedExtent(ForceInitToZero)
	, CachedBrushType(EBrushType::Brush_Default)
	, CachedSurfaceHash(0)
{
}

FHoudiniBrushInfo::FHoudiniBrushInfo(ABrush* InBrushActor)
{
	if (!InBrushActor)
		return;

	BrushActor = InBrushActor;
	CachedTransform = BrushActor->GetActorTransform();
	BrushActor->GetActorBounds(false, CachedOrigin, CachedExtent);
	CachedBrushType = BrushActor->BrushType;

#if WITH_EDITOR
	UModel* Model = BrushActor->Brush;

	// Cache the hash of the surface properties
	if (IsValid(Model) && IsValid(Model->Polys))
	{
		int32 NumPolys = Model->Polys->Element.Num();
		CachedSurfaceHash = 0;
		for(int32 iPoly = 0; iPoly < NumPolys; ++iPoly)
		{
			const FPoly& Poly = Model->Polys->Element[iPoly]; 
			CombinePolyHash(CachedSurfaceHash, Poly);
		}
	}
	else
	{
		CachedSurfaceHash = 0;
	}
#endif
}

bool FHoudiniBrushInfo::HasChanged() const
{
	if (!BrushActor.IsValid())
		return false;

	// Has the transform changed?
	if (!BrushActor->GetActorTransform().Equals(CachedTransform))
		return true;

	if (BrushActor->BrushType != CachedBrushType)
		return true;

	// Has the actor bounds changed?
	FVector TmpOrigin, TmpExtent;
	BrushActor->GetActorBounds(false, TmpOrigin, TmpExtent);

	if (!(TmpOrigin.Equals(CachedOrigin) && TmpExtent.Equals(CachedExtent) ))
		return true;
#if WITH_EDITOR
	// Is there a tracked surface property that changed?
	UModel* Model = BrushActor->Brush;
	if (IsValid(Model) && IsValid(Model->Polys))
	{
		// Hash the incoming surface properties and compared it against the cached hash.
		int32 NumPolys = Model->Polys->Element.Num();
		uint64 SurfaceHash = 0;
		for (int32 iPoly = 0; iPoly < NumPolys; ++iPoly)
		{
			const FPoly& Poly = Model->Polys->Element[iPoly];
			CombinePolyHash(SurfaceHash, Poly);
		}
		if (SurfaceHash != CachedSurfaceHash)
			return true;
	}
	else
	{
		if (CachedSurfaceHash != 0)
			return true;
	}
#endif
	return false;
}

int32 FHoudiniBrushInfo::GetNumVertexIndicesFromModel(const UModel* Model)
{
	const TArray<FBspNode>& Nodes = Model->Nodes;		
	int32 NumIndices = 0;
	// Build the face counts buffer by iterating over the BSP nodes.
	for(const FBspNode& Node : Nodes)
	{
		NumIndices += Node.NumVertices;
	}
	return NumIndices;
}

UModel* UHoudiniInputBrush::GetCachedModel() const
{
	return CombinedModel;
}

bool UHoudiniInputBrush::HasBrushesChanged(const TArray<ABrush*>& InBrushes) const
{
	if (InBrushes.Num() != BrushesInfo.Num())
		return true;

	int32 NumBrushes = BrushesInfo.Num();

	for (int32 InfoIndex = 0; InfoIndex < NumBrushes; ++InfoIndex)
	{
		const FHoudiniBrushInfo& BrushInfo = BrushesInfo[InfoIndex];
		// Has the cached brush actor invalid?
		if (!BrushInfo.BrushActor.IsValid())
			return true;

		// Has there been an order change in the actors list?
		if (InBrushes[InfoIndex] != BrushInfo.BrushActor.Get())
			return true;

		// Has there been any other changes to the brush?
		if (BrushInfo.HasChanged())
			return true;
	}

	// Nothing has changed.
	return false;
}

void UHoudiniInputBrush::UpdateCachedData(UModel* InCombinedModel, const TArray<ABrush*>& InBrushes)
{
	ABrush* InputBrush = GetBrush();
	if (IsValid(InputBrush))
	{
		CachedInputBrushType = InputBrush->BrushType;
	}

	// Cache the combined model aswell as the brushes used to generate this model.
	CombinedModel = InCombinedModel;

	BrushesInfo.SetNum(InBrushes.Num());
	for (int i = 0; i < InBrushes.Num(); ++i)
	{
		if (!InBrushes[i])
			continue;
		BrushesInfo[i] = FHoudiniBrushInfo(InBrushes[i]);
	}
}


void
UHoudiniInputBrush::Update(UObject * InObject)
{
	Super::Update(InObject);

	ABrush* BrushActor = GetBrush();
	if (!IsValid(BrushActor))
	{
		bIgnoreInputObject = true;
		return;
	}

	CachedInputBrushType = BrushActor->BrushType;

	bIgnoreInputObject = ShouldIgnoreThisInput();
}

bool
UHoudiniInputBrush::ShouldIgnoreThisInput()
{
	// Invalid brush, should be ignored
	ABrush* BrushActor = GetBrush();
	ensure(BrushActor);
	if (!BrushActor)
		return true;

	// If the BrushType has changed since caching this object, this object cannot be ignored.
	if (CachedInputBrushType != BrushActor->BrushType)
		return false;

	// If it's not an additive brush, we want to ignore it
	bool bShouldBeIgnored = BrushActor->BrushType != EBrushType::Brush_Add;

	// If this is not a static brush (e.g., AVolume), ignore it.
	if (!bShouldBeIgnored)
		bShouldBeIgnored = !BrushActor->IsStaticBrush();

	return bShouldBeIgnored;
}

bool UHoudiniInputBrush::HasContentChanged() const
{
	ABrush* BrushActor = GetBrush();
	ensure(BrushActor);
	
	if (!BrushActor)
		return false;

	if (BrushActor->BrushType != CachedInputBrushType)
		return true;
	
	if (bIgnoreInputObject)
		return false;

	// Find intersecting actors and capture their properties so that
	// we can determine whether something has changed.
	TArray<ABrush*> IntersectingBrushes;
	FindIntersectingSubtractiveBrushes(this, IntersectingBrushes);
	
	if (HasBrushesChanged(IntersectingBrushes))
	{
		return true;
	}

	return false;
}

bool
UHoudiniInputBrush::HasActorTransformChanged() const
{
	if (bIgnoreInputObject)
		return false;

	return Super::HasActorTransformChanged();
}


bool UHoudiniInputBrush::FindIntersectingSubtractiveBrushes(const UHoudiniInputBrush* InputBrush, TArray<ABrush*>& OutBrushes)
{
	TArray<AActor*> IntersectingActors;	
	TArray<FBox> Bounds;


	if (!IsValid(InputBrush))
		return false;

	ABrush* BrushActor = InputBrush->GetBrush();
	if (!IsValid(BrushActor))
		return false;


	OutBrushes.Empty();

	Bounds.Add( BrushActor->GetComponentsBoundingBox(true, true) );

	FHoudiniEngineRuntimeUtils::FindActorsOfClassInBounds(BrushActor->GetWorld(), ABrush::StaticClass(), Bounds, nullptr, IntersectingActors);

	//--------------------------------------------------------------------------------------------------
	// Filter the actors to only keep intersecting subtractive brushes.
	//--------------------------------------------------------------------------------------------------
	for (AActor* Actor : IntersectingActors)
	{
		// Filter out anything that is not a static brush (typically volume actors).
		ABrush* Brush = Cast<ABrush>(Actor);

		// NOTE: The brush actor needs to be added in the correct map/level order
		// together with the subtractive brushes otherwise the CSG operations 
		// will not match the BSP in the level.
		if (Actor == BrushActor)
			OutBrushes.Add(Brush);

		if (!(Brush && Brush->IsStaticBrush()))
			continue;

		if (Brush->BrushType == Brush_Subtract)
			OutBrushes.Add(Brush);
	}

	return true;	
}

#if WITH_EDITOR
void 
UHoudiniInputObject::PostEditUndo()
{
	Super::PostEditUndo();
	MarkChanged(true);
}
#endif

UHoudiniInputObject*
UHoudiniInputObject::DuplicateAndCopyState(UObject * DestOuter)
{
	UHoudiniInputObject* NewInput = Cast<UHoudiniInputObject>(StaticDuplicateObject(this, DestOuter));
	NewInput->CopyStateFrom(this, false);
	return NewInput;
}

void 
UHoudiniInputObject::CopyStateFrom(UHoudiniInputObject* InInput, bool bCopyAllProperties)
{
	// Copy the state of this UHoudiniInput object.
	if (bCopyAllProperties)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References should be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InInput, this, Params);
	}

	InputNodeId = InInput->InputNodeId;
	InputObjectNodeId = InInput->InputObjectNodeId;
	bHasChanged = InInput->bHasChanged;
	bNeedsToTriggerUpdate = InInput->bNeedsToTriggerUpdate;
	bTransformChanged = InInput->bTransformChanged;
	Guid = InInput->Guid;

#if WITH_EDITORONLY_DATA
	bUniformScaleLocked = InInput->bUniformScaleLocked;
#endif

}

void
UHoudiniInputObject::SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes)
{
	bCanDeleteHoudiniNodes = bInCanDeleteNodes;
}

bool
UHoudiniInputObject::GetChangedObjectsAndValidNodes(TArray<UHoudiniInputObject*>& OutChangedObjects, TArray<int32>& OutNodeIdsOfUnchangedValidObjects)
{
	if (HasChanged())
	{
		// Has changed, needs to be recreated
		OutChangedObjects.Add(this);

		return true;
	}
	
	if (UsesInputObjectNode())
	{
		if (InputObjectNodeId >= 0)
		{
			// No changes, keep it
			OutNodeIdsOfUnchangedValidObjects.Add(InputObjectNodeId);
		}
		else
		{
			// Needs, but does not have, a current HAPI input node
			OutChangedObjects.Add(this);

			return true;
		}
	}

	// No changes and valid object node exists (or no node is used by this object type)
	return false;
}

//
UHoudiniInputDataTable::UHoudiniInputDataTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniInputObject *
UHoudiniInputDataTable::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_DT_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputDataTable::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputDataTable * HoudiniInputObject = NewObject<UHoudiniInputDataTable>(
		InOuter, UHoudiniInputDataTable::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::DataTable;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

UDataTable*
UHoudiniInputDataTable::GetDataTable() const
{
	return Cast<UDataTable>(InputObject.LoadSynchronous());
}

UHoudiniInputFoliageType_InstancedStaticMesh::UHoudiniInputFoliageType_InstancedStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UHoudiniInputObject*
UHoudiniInputFoliageType_InstancedStaticMesh::Create(UObject * InObject, UObject* InOuter, const FString& InName)
{
	FString InputObjectNameStr = "HoudiniInputObject_FoliageSM_" + InName;
	FName InputObjectName = MakeUniqueObjectName(InOuter, UHoudiniInputFoliageType_InstancedStaticMesh::StaticClass(), *InputObjectNameStr);

	// We need to create a new object
	UHoudiniInputFoliageType_InstancedStaticMesh * HoudiniInputObject = NewObject<UHoudiniInputFoliageType_InstancedStaticMesh>(
		InOuter, UHoudiniInputFoliageType_InstancedStaticMesh::StaticClass(), InputObjectName, RF_Public | RF_Transactional);

	HoudiniInputObject->Type = EHoudiniInputObjectType::FoliageType_InstancedStaticMesh;
	HoudiniInputObject->Update(InObject);
	HoudiniInputObject->bHasChanged = true;

	return HoudiniInputObject;
}

void
UHoudiniInputFoliageType_InstancedStaticMesh::CopyStateFrom(UHoudiniInputObject* InInput, bool bCopyAllProperties)
{
	UHoudiniInputFoliageType_InstancedStaticMesh* FoliageTypeSM = Cast<UHoudiniInputFoliageType_InstancedStaticMesh>(InInput); 
	if (!IsValid(FoliageTypeSM))
		return;

	UHoudiniInputObject::CopyStateFrom(FoliageTypeSM, bCopyAllProperties);

	// BlueprintStaticMeshes array is not used in UHoudiniInputFoliageType_InstancedStaticMesh
	BlueprintStaticMeshes.Empty();
}

void
UHoudiniInputFoliageType_InstancedStaticMesh::Update(UObject * InObject)
{
	UHoudiniInputObject::Update(InObject);
	UFoliageType_InstancedStaticMesh* const FoliageType = Cast<UFoliageType_InstancedStaticMesh>(InObject);
	ensure(FoliageType);
	ensure(FoliageType->GetStaticMesh());
}

UStaticMesh*
UHoudiniInputFoliageType_InstancedStaticMesh::GetStaticMesh() const
{
	if (!InputObject.IsValid())
		return nullptr;
	
	UFoliageType_InstancedStaticMesh* const FoliageType = Cast<UFoliageType_InstancedStaticMesh>(InputObject.LoadSynchronous());
	if (!IsValid(FoliageType))
		return nullptr;

	return FoliageType->GetStaticMesh();
}
