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

#include "DetailCategoryBuilder.h"
#if defined(HOUDINI_USE_PCG)

#include "HoudiniPCGComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"

// Slate UI elements
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// Debugging (for on-screen messages)
#include "Engine/Engine.h"

UHoudiniPCGComponent::UHoudiniPCGComponent(class FObjectInitializer const& ObjectInitializer)
{
}

void UHoudiniPCGComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{

}

UHoudiniPCGComponent* UHoudiniPCGComponent::CreatePCGComponent(UPCGComponent* UnrealPCComponent)
{
	const bool bUseHoudiniActor = true;

	USceneComponent* RootComponent = UnrealPCComponent->GetOwner()->GetRootComponent();
	AActor* Owner = UnrealPCComponent->GetOwner();

	if(bUseHoudiniActor)
	{
		UWorld* World = UnrealPCComponent->GetWorld();
		AHoudiniPCGActor* HoudiniPCGActor = nullptr;
		for(TActorIterator<AHoudiniPCGActor> It(World); It; ++It)
		{
			HoudiniPCGActor = *It;
			if(HoudiniPCGActor)
				break;
		}

		if(HoudiniPCGActor == nullptr)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = nullptr;
			SpawnParams.Instigator = nullptr;
			FVector SpawnLocation = UnrealPCComponent->GetOwner()->GetActorLocation();
			FRotator SpawnRotation = UnrealPCComponent->GetOwner()->GetActorRotation();

			HoudiniPCGActor = World->SpawnActor<AHoudiniPCGActor>(AHoudiniPCGActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
		}

		RootComponent = HoudiniPCGActor->GetRootComponent();
		if(!RootComponent)
		{
			RootComponent = NewObject< USceneComponent>(HoudiniPCGActor);
			HoudiniPCGActor->SetRootComponent(RootComponent);
			HoudiniPCGActor->AddInstanceComponent(RootComponent);
		}
	}

	UHoudiniPCGComponent* PCGComponent = NewObject<UHoudiniPCGComponent>(RootComponent);
	FTransform ComponentTransform = UnrealPCComponent->GetOwner()->GetTransform();
	ComponentTransform.SetScale3D(FVector3d::One());
	PCGComponent->SetWorldTransform(ComponentTransform);
	PCGComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	Owner->AddInstanceComponent(PCGComponent);
	PCGComponent->RegisterComponent();
	PCGComponent->PCGComponent = UnrealPCComponent;

	return PCGComponent;
}


TSharedRef<IDetailCustomization> UHoudiniPCGComponentDetails::MakeInstance()
{
	return MakeShareable(new UHoudiniPCGComponentDetails);
}

void UHoudiniPCGComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const FName PCGCategoryName("PCG");
	IDetailCategoryBuilder& PCGCategory = DetailBuilder.EditCategory(PCGCategoryName);

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	TArray<UHoudiniPCGComponent*> Components;

	for(TWeakObjectPtr<UObject> Obj : ObjectsBeingCustomized)
	{
		if(Obj.IsValid())
		{
			UHoudiniPCGComponent* Component = Cast<UHoudiniPCGComponent>(Obj.Get());
			if(IsValid(Component))
				Components.Add(Component);
		}
	}

	for (UHoudiniPCGComponent * Component : Components)
	{
		IDetailCategoryBuilder& MyCategory = DetailBuilder.EditCategory("Custom Category");

		MyCategory.AddCustomRow(FText::FromString("Custom Button"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(FText::FromString("Action Button"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SButton)
					.Text(FText::FromString("Click Me"))
					.OnClicked_Lambda([Component]()
						{
							if(GEngine)
							{
								// TODO: Still needed? 
							}
							return FReply::Handled();
						})
			];
	}

}

UHoudiniPCGComponentDetails::UHoudiniPCGComponentDetails()
{
}


#endif