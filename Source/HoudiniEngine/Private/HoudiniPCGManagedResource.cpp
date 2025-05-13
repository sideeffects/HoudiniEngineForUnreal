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

#include "HoudiniPCGManagedResource.h"
#include "HoudiniPCGComponent.h"
#include "PCGGraph.h"

void UHoudiniPCGManagedResource::PostEditImport()
{
	// In this case, the managed actors won't be copied along the actor/component,
	// So we just have to "forget" the actors.
	Super::PostEditImport();
}

void UHoudiniPCGManagedResource::PostApplyToComponent()
{
	// In this case, we want to preserve the data, so we need to do nothing
}

void UHoudiniPCGManagedResource::DestroyCookable()
{
	if(IsValid(HoudiniPCGComponent))
	{
		if(IsValid(HoudiniPCGComponent->Cookable))
			HoudiniPCGComponent->Cookable->DestroyCookable(HoudiniPCGComponent->GetWorld());
		HoudiniPCGComponent->Cookable = nullptr;
	}

}

bool UHoudiniPCGManagedResource::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& /*OutActorsToDelete*/)
{
	bIsMarkedUnused = true;
	if (bHardRelease)
	{
		if(IsValid(HoudiniPCGComponent))
		{
			DestroyCookable();


			AActor* Owner = HoudiniPCGComponent->GetOwner();
			HoudiniPCGComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			HoudiniPCGComponent->DestroyComponent();
			GEditor->NoteSelectionChange();

			HoudiniPCGComponent = nullptr;
		}
	}
	if (PCGComponent)
	{
		PCGComponent->GetGraph()->OnGraphChangedDelegate.RemoveAll(this);
	}
	return bHardRelease;
}

void UHoudiniPCGManagedResource::OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	// We're really look for "Force Regenerate", but there are no such flags.
	const bool bIsStructural = ((ChangeType & (EPCGChangeType::Edge | EPCGChangeType::Structural)) != EPCGChangeType::None);

	if (bIsStructural)
		bInvalidateResource = true;
}

bool UHoudiniPCGManagedResource::ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	if(bIsMarkedUnused)
	{
		Release(true, OutActorsToDelete);
		return true;
	}

	return false;
}

void UHoudiniPCGManagedResource::MarkAsUsed()
{
	Super::MarkAsUsed();
}

void UHoudiniPCGManagedResource::MarkAsReused()
{
	Super::MarkAsReused();
}

void UHoudiniPCGManagedResource::PostLoad()
{
	Super::PostLoad();

	// Create a new cookable after deserializing.
	bInvalidateResource = true;
}

bool UHoudiniPCGManagedResource::MoveResourceToNewActor(AActor* NewActor)
{
	return Super::MoveResourceToNewActor(NewActor);
}

#if WITH_EDITOR
void UHoudiniPCGManagedResource::ChangeTransientState(EPCGEditorDirtyMode NewEditingMode)
{
	Super::ChangeTransientState(NewEditingMode);
}
#endif


