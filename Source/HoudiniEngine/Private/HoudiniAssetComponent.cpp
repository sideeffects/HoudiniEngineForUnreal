/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEnginePrivatePCH.h"
#include <stdint.h>


#define HOUDINI_TEST_LOG_MESSAGE( NameWithSpaces )


UScriptStruct*
UHoudiniAssetComponent::ScriptStructColor = nullptr;


uint32
UHoudiniAssetComponent::ComponentPatchedClassCounter = 0u;


bool
UHoudiniAssetComponent::bDisplayEngineNotInitialized = true;


// Define accessor for UObjectBase::SetClass private method.
struct FPrivate_UObjectBase_SetClass
{
	typedef void (UObjectBase::*type)(UClass*);
};

HOUDINI_PRIVATE_PATCH(FPrivate_UObjectBase_SetClass, UObjectBase::SetClass);

// Define accessor for UClass::CreateDefaultObject private method.
struct FPrivate_UClass_CreateDefaultObject
{
	typedef UObject* (UClass::*type)();
};

HOUDINI_PRIVATE_PATCH(FPrivate_UClass_CreateDefaultObject, UClass::CreateDefaultObject);


UHoudiniAssetComponent::UHoudiniAssetComponent(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	HoudiniAsset(nullptr),
	ChangedHoudiniAsset(nullptr),
	PatchedClass(nullptr),
	AssetId(-1),
	InputCount(0),
	HapiNotificationStarted(0.0),
	bContainsHoudiniLogoGeometry(false),
	bIsNativeComponent(false),
	bIsPreviewComponent(false),
	bLoadedComponent(false),
	bLoadedComponentRequiresInstantiation(false),
	bInstantiated(false),
	bIsPlayModeActive(false)
{
	UObject* Object = PCIP.GetObject();
	UObject* ObjectOuter = Object->GetOuter();

	if(ObjectOuter->IsA(AHoudiniAssetActor::StaticClass()))
	{
		bIsNativeComponent = true;
	}

	// Set component properties.
	Mobility = EComponentMobility::Movable;
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bGenerateOverlapEvents = false;

	// Similar to UMeshComponent.
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;

	// This component requires render update.
	bNeverNeedsRenderUpdate = false;

	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID.Invalidate();

	// Zero scratch space.
	FMemory::Memset(ScratchSpaceBuffer, 0x0, HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE);

	HOUDINI_TEST_LOG_MESSAGE( "Constructor,                          C" );
}


UHoudiniAssetComponent::~UHoudiniAssetComponent()
{
	//HOUDINI_TEST_LOG_MESSAGE( "Destructor,                           C" );
}


void
UHoudiniAssetComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// We need to make sure the component class has been patched.
	UClass* ObjectClass = InThis->GetClass();

	if(UHoudiniAssetComponent::StaticClass() != ObjectClass)
	{
		if(ObjectClass->ClassAddReferencedObjects == UHoudiniAssetComponent::AddReferencedObjects)
		{
			// This is a safe cast since our component is the only type registered for this callback.
			UHoudiniAssetComponent* HoudiniAssetComponent = (UHoudiniAssetComponent*)InThis;
			if(HoudiniAssetComponent && !HoudiniAssetComponent->IsPendingKill())
			{
				// If we have patched class object, add it as referenced.
				UClass* PatchedClass = HoudiniAssetComponent->PatchedClass;
				if(PatchedClass)
				{
					Collector.AddReferencedObject(PatchedClass, InThis);
				}

				// Retrieve asset associated with this component.
				//UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
				//if(HoudiniAsset)
				//{
				//	// Manually add a reference to Houdini asset from this component.
				//	Collector.AddReferencedObject(HoudiniAsset, InThis);
				//}

				// Add references to all static meshes and their static mesh components.
				for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(HoudiniAssetComponent->StaticMeshComponents); Iter; ++Iter)
				{
					UStaticMesh* StaticMesh = Iter.Key();
					UStaticMeshComponent* StaticMeshComponent = Iter.Value();

					Collector.AddReferencedObject(StaticMesh, InThis);
					Collector.AddReferencedObject(StaticMeshComponent, InThis);
				}
			}
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetComponent::SetNative(bool InbIsNativeComponent)
{
	bIsNativeComponent = InbIsNativeComponent;
}


HAPI_AssetId
UHoudiniAssetComponent::GetAssetId() const
{
	return AssetId;
}


void
UHoudiniAssetComponent::SetAssetId(HAPI_AssetId InAssetId)
{
	AssetId = InAssetId;
}


UHoudiniAsset*
UHoudiniAssetComponent::GetHoudiniAsset() const
{
	return HoudiniAsset;
}


AHoudiniAssetActor*
UHoudiniAssetComponent::GetHoudiniAssetActorOwner() const
{
	return Cast<AHoudiniAssetActor>(GetOwner());
}


void
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	// If it is the same asset, do nothing.
	if(InHoudiniAsset == HoudiniAsset)
	{
		return;
	}

	HOUDINI_TEST_LOG_MESSAGE( "  SetHoudiniAsset(Before),            C" );

	UHoudiniAsset* Asset = nullptr;
	AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(GetOwner());

	HoudiniAsset = InHoudiniAsset;

	// Set Houdini logo to be default geometry.
	ReleaseStaticMeshResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();
	CreateStaticMeshHoudiniLogoResource();

	bIsPreviewComponent = false;
	if(!InHoudiniAsset)
	{
		HOUDINI_TEST_LOG_MESSAGE( "  SetHoudiniAsset(After),             C" );
		return;
	}
	
	if(HoudiniAssetActor)
	{
		bIsPreviewComponent = HoudiniAssetActor->IsUsedForPreview();
	}

	if(!bIsNativeComponent)
	{
		bLoadedComponent = false;
	}

	// Get instance of Houdini Engine.
	FHoudiniEngine& HoudiniEngine = FHoudiniEngine::Get();

	// If this is first time component is instantiated and we do not have Houdini Engine initialized, display message.
	if(!bIsPreviewComponent && !HoudiniEngine.IsInitialized() && UHoudiniAssetComponent::bDisplayEngineNotInitialized)
	{
		int RunningEngineMajor = 0;
		int RunningEngineMinor = 0;
		int RunningEngineApi = 0;

		// Retrieve version numbers for running Houdini Engine.
		HAPI_GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
		HAPI_GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
		HAPI_GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

		FString WarningMessage = FString::Printf(TEXT("Build version: %d.%d.api:%d vs Running version: %d.%d.api:%d mismatch. ")
												 TEXT("Is your PATH correct? Please update it to match Build version. ")
												 TEXT("No cooking / instantiation will take place."),
													HAPI_VERSION_HOUDINI_ENGINE_MAJOR,
													HAPI_VERSION_HOUDINI_ENGINE_MINOR,
													HAPI_VERSION_HOUDINI_ENGINE_API,
													RunningEngineMajor,
													RunningEngineMinor,
													RunningEngineApi);

		FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
		FText WarningTitleText = FText::FromString(WarningTitle);
		FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);
		UHoudiniAssetComponent::bDisplayEngineNotInitialized = false;
	}

	if(!bIsPreviewComponent && !bLoadedComponent)
	{
		EHoudiniEngineTaskType::Type HoudiniEngineTaskType = EHoudiniEngineTaskType::AssetInstantiation;

		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		FHoudiniEngineTask Task(HoudiniEngineTaskType, HapiGUID);
		Task.Asset = InHoudiniAsset;
		Task.ActorName = GetOuter()->GetName();
		HoudiniEngine.AddTask(Task);

		// Start ticking - this will poll the cooking system for completion.
		StartHoudiniTicking();
	}

	HOUDINI_TEST_LOG_MESSAGE( "  SetHoudiniAsset(After),             C" );
}


void
UHoudiniAssetComponent::AssignUniqueActorLabel()
{
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		AHoudiniAssetActor* HoudiniAssetActor = GetHoudiniAssetActorOwner();
		if(HoudiniAssetActor)
		{
			FString UniqueName;
			if(FHoudiniEngineUtils::GetHoudiniAssetName(AssetId, UniqueName))
			{
				GEditor->SetActorLabelUnique(HoudiniAssetActor, UniqueName);
			}
		}
	}
}


void
UHoudiniAssetComponent::CreateStaticMeshResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	// Reset Houdini logo flag.
	bContainsHoudiniLogoGeometry = false;

	// Reset array used for static mesh preview.
	PreviewStaticMeshes.Empty();

	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshMap); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(StaticMesh)
		{
			// See if we need to create component for this mesh.
			if(HoudiniGeoPartObject.IsVisible())
			{
				UStaticMeshComponent* StaticMeshComponent = nullptr;
				UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);

				if(FoundStaticMeshComponent)
				{
					StaticMeshComponent = *FoundStaticMeshComponent;
				}
				else
				{
					// Create necessary component.
					StaticMeshComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), GetOwner(), NAME_None, RF_Transient);

					// Add to map of components.
					StaticMeshComponents.Add(StaticMesh, StaticMeshComponent);

					StaticMeshComponent->AttachTo(this);
					StaticMeshComponent->RegisterComponent();
					StaticMeshComponent->SetStaticMesh(StaticMesh);
					StaticMeshComponent->SetVisibility(true);
				}

				// Transform the component by transformation provided by HAPI.
				StaticMeshComponent->SetRelativeTransform(FTransform(HoudiniGeoPartObject.TransformMatrix));
			}

			// Add static mesh to preview list.
			PreviewStaticMeshes.Add(StaticMesh);
		}
		else if(HoudiniGeoPartObject.IsInstancer())
		{
			// This geo part is an instancer.
		}
	}

	// Skip self assignment.
	if(&StaticMeshes != &StaticMeshMap)
	{
		StaticMeshes = StaticMeshMap;
	}
}


void
UHoudiniAssetComponent::ReleaseStaticMeshResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	UStaticMesh* HoudiniLogoStaticMesh = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh();

	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshMap); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(StaticMesh)
		{
			// Locate corresponding component.
			UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);
			if(FoundStaticMeshComponent)
			{
				// Remove component from map of static mesh components.
				StaticMeshComponents.Remove(StaticMesh);

				// Detach and destroy the component.
				UStaticMeshComponent* StaticMeshComponent = *FoundStaticMeshComponent;
				StaticMeshComponent->DetachFromParent();
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->DestroyComponent();
			}
		}
	}

	StaticMeshMap.Empty();
}


void
UHoudiniAssetComponent::StartHoudiniTicking()
{
	HOUDINI_TEST_LOG_MESSAGE( "  StartHoudiniTicking,                C" );

	// If we have no timer delegate spawned for this component, spawn one.
	if(!TimerDelegateCooking.IsBound())
	{
		TimerDelegateCooking = FTimerDelegate::CreateUObject(this, &UHoudiniAssetComponent::TickHoudiniComponent);

		// We need to register delegate with the timer system.
		static const float TickTimerDelay = 0.25f;
		GEditor->GetTimerManager()->SetTimer(TimerDelegateCooking, TickTimerDelay, true);

		// Grab current time for delayed notification.
		HapiNotificationStarted = FPlatformTime::Seconds();
	}
}


void
UHoudiniAssetComponent::StopHoudiniTicking()
{
	HOUDINI_TEST_LOG_MESSAGE( "  StopHoudiniTicking,                 C" );

	if(TimerDelegateCooking.IsBound())
	{
		GEditor->GetTimerManager()->ClearTimer(TimerDelegateCooking);
		TimerDelegateCooking.Unbind();

		// Reset time for delayed notification.
		HapiNotificationStarted = 0.0;
	}
}


void
UHoudiniAssetComponent::StartHoudiniAssetChange()
{
	HOUDINI_TEST_LOG_MESSAGE( "  StartHoudiniAssetChange,            C" );

	// If we have no timer delegate spawned for this component, spawn one.
	if(!TimerDelegateAssetChange.IsBound())
	{
		TimerDelegateAssetChange = FTimerDelegate::CreateUObject(this, &UHoudiniAssetComponent::TickHoudiniAssetChange);

		// We need to register delegate with the timer system.
		static const float TickTimerDelay = 0.01f;
		GEditor->GetTimerManager()->SetTimer(TimerDelegateAssetChange, TickTimerDelay, false);
	}
}


void
UHoudiniAssetComponent::StopHoudiniAssetChange()
{
	HOUDINI_TEST_LOG_MESSAGE( "  StopHoudiniAssetChange,             C" );

	if(TimerDelegateAssetChange.IsBound())
	{
		GEditor->GetTimerManager()->ClearTimer(TimerDelegateAssetChange);
		TimerDelegateAssetChange.Unbind();
	}
}


void
UHoudiniAssetComponent::TickHoudiniComponent()
{
	FHoudiniEngineTaskInfo TaskInfo;
	bool bStopTicking = false;

	static float NotificationFadeOutDuration = 2.0f;
	static float NotificationExpireDuration = 2.0f;
	static double NotificationUpdateFrequency = 2.0f;

	// Check if Houdini Asset has changed, if it did we need to 

	if(HapiGUID.IsValid())
	{
		// If we have a valid task GUID.

		if(FHoudiniEngine::Get().RetrieveTaskInfo(HapiGUID, TaskInfo))
		{
			if(EHoudiniEngineTaskState::None != TaskInfo.TaskState)
			{
				if(!NotificationPtr.IsValid())
				{
					FNotificationInfo Info(TaskInfo.StatusText);

					Info.bFireAndForget = false;
					Info.FadeOutDuration = NotificationFadeOutDuration;
					Info.ExpireDuration = NotificationExpireDuration;

					TSharedPtr<FSlateDynamicImageBrush> HoudiniBrush = FHoudiniEngine::Get().GetHoudiniLogoBrush();
					if(HoudiniBrush.IsValid())
					{
						Info.Image = HoudiniBrush.Get();
					}

					if((FPlatformTime::Seconds() - HapiNotificationStarted) >= NotificationUpdateFrequency)
					{
						NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
					}
				}
			}

			switch(TaskInfo.TaskState)
			{
				case EHoudiniEngineTaskState::FinishedInstantiation:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedInstantiation."));

					if(FHoudiniEngineUtils::IsValidAssetId(TaskInfo.AssetId))
					{
						// Set new asset id.
						SetAssetId(TaskInfo.AssetId);

						// Compute number of inputs.
						//ComputeInputCount();

						// Assign unique actor label based on asset name.
						AssignUniqueActorLabel();

						if(NotificationPtr.IsValid())
						{
							TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
							if(NotificationItem.IsValid())
							{
								NotificationItem->SetText(TaskInfo.StatusText);
								NotificationItem->ExpireAndFadeout();

								NotificationPtr.Reset();
							}
						}
						FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
						HapiGUID.Invalidate();

						// We just finished instantiation, we need to schedule a cook.
						bInstantiated = true;
					}
					else
					{
						bStopTicking = true;
						HOUDINI_LOG_MESSAGE(TEXT("    Received invalid asset id."));
					}

					break;
				}

				case EHoudiniEngineTaskState::FinishedCooking:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedCooking."));

					if(FHoudiniEngineUtils::IsValidAssetId(TaskInfo.AssetId))
					{
						// Set new asset id.
						SetAssetId(TaskInfo.AssetId);

						// Compute number of inputs.
						ComputeInputCount();

						// Assign unique actor label based on asset name.
						//AssignUniqueActorLabel();

						{
							TMap<FHoudiniGeoPartObject, UStaticMesh*> NewStaticMeshes;
							if(FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(AssetId, HoudiniAsset, nullptr, StaticMeshes, NewStaticMeshes))
							{
								// Remove all duplicates. After this operation, old map will have meshes which we need to deallocate.
								for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(NewStaticMeshes); Iter; ++Iter)
								{
									const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
									UStaticMesh* StaticMesh = Iter.Value();

									// Remove mesh from previous map of meshes.
									int32 ObjectsRemoved = StaticMeshes.Remove(HoudiniGeoPartObject);
								}

								// Free meshes and components that are no longer used.
								ReleaseStaticMeshResources(StaticMeshes);

								// Set meshes and create new components for those meshes that do not have them.
								CreateStaticMeshResources(NewStaticMeshes);
							}
						}

						// We need to patch component RTTI to reflect properties for this component.
						ReplaceClassInformation(GetOuter()->GetName());

						// Need to update rendering information.
						UpdateRenderingInformation();

						// Force editor to redraw viewports.
						if(GEditor)
						{
							GEditor->RedrawAllViewports();
						}

						// Update properties panel after instantiation.
						if(bInstantiated)
						{
							UpdateEditorProperties();
						}
					}
					else
					{
						HOUDINI_LOG_MESSAGE(TEXT("    Received invalid asset id."));
					}

					if(NotificationPtr.IsValid())
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
							NotificationItem->ExpireAndFadeout();

							NotificationPtr.Reset();
						}
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();
					bStopTicking = true;
					bInstantiated = false;

					break;
				}

				case EHoudiniEngineTaskState::FinishedCookingWithErrors:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedCookingWithErrors."));

					if(FHoudiniEngineUtils::IsValidAssetId(TaskInfo.AssetId))
					{
						// Compute number of inputs.
						ComputeInputCount();

						// We need to patch component RTTI to reflect properties for this component.
						ReplaceClassInformation(GetOuter()->GetName());

						// Update properties panel.
						UpdateEditorProperties();
					}
				}

				case EHoudiniEngineTaskState::Aborted:
				case EHoudiniEngineTaskState::FinishedInstantiationWithErrors:
				{
					HOUDINI_LOG_MESSAGE(TEXT("    FinishedInstantiationWithErrors."));

					if(NotificationPtr.IsValid())
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
							NotificationItem->ExpireAndFadeout();

							NotificationPtr.Reset();
						}
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();
					bStopTicking = true;
					bInstantiated = false;

					break;
				}

				case EHoudiniEngineTaskState::Processing:
				{

					if(NotificationPtr.IsValid())
					{
						TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
						if(NotificationItem.IsValid())
						{
							NotificationItem->SetText(TaskInfo.StatusText);
						}
					}

					break;
				}

				case EHoudiniEngineTaskState::None:
				default:
				{
					break;
				}
			}
		}
		else
		{
			// Task information does not exist, we can stop ticking.
			HapiGUID.Invalidate();
			bStopTicking = true;
		}
	}

	if(!HapiGUID.IsValid() && (bInstantiated || (ChangedProperties.Num() > 0)))
	{
		// If we are not cooking and we have property changes queued up.

		// Grab current time for delayed notification.
		HapiNotificationStarted = FPlatformTime::Seconds();

		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		if(bLoadedComponentRequiresInstantiation)
		{
			bLoadedComponentRequiresInstantiation = false;

			FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetInstantiation, HapiGUID);
			Task.Asset = HoudiniAsset;
			Task.ActorName = GetOuter()->GetName();
			FHoudiniEngine::Get().AddTask(Task);
		}
		else
		{
			// We need to set all property values.
			SetChangedPropertyValues();

			HAPI_AssetInfo AssetInfo;
			if((HAPI_RESULT_SUCCESS == HAPI_GetAssetInfo(AssetId, &AssetInfo)) && AssetInfo.hasEverCooked)
			{
				// Remove all processed parameters.
				ClearChangedPropertiesAll();
			}
			else
			{
				// Asset has been instantiated, but not cooked. We cannot submit inputs yet.
				ClearChangedPropertiesParameters();
			}

			// Create asset instantiation task object and submit it for processing.
			FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, HapiGUID);
			Task.ActorName = GetOuter()->GetName();
			Task.AssetComponent = this;
			FHoudiniEngine::Get().AddTask(Task);
		}

		// We do not want to stop ticking system as we have just submitted a task.
		bStopTicking = false;
	}

	if(bStopTicking)
	{
		StopHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::TickHoudiniAssetChange()
{
	// We need to remove properties from generated class and restore original component information.
	if(PatchedClass)
	{
		RemoveClassProperties(PatchedClass);
		PatchedClass = nullptr;
	}

	// Restore component class to be the default Houdini component class.
	ReplaceClassObject(UHoudiniAssetComponent::StaticClass());

	// We need to update editor properties.
	UpdateEditorProperties();

	if(HoudiniAsset)
	{
		EHoudiniEngineTaskType::Type HoudiniEngineTaskType = EHoudiniEngineTaskType::AssetInstantiation;

		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		FHoudiniEngineTask Task(HoudiniEngineTaskType, HapiGUID);
		Task.Asset = HoudiniAsset;
		Task.ActorName = GetOuter()->GetName();
		FHoudiniEngine::Get().AddTask(Task);

		// Start ticking - this will poll the cooking system for completion.
		StartHoudiniTicking();
	}

	// We no longer need this ticker.
	StopHoudiniAssetChange();
}


void
UHoudiniAssetComponent::UpdateEditorProperties()
{
	AHoudiniAssetActor* HoudiniAssetActor = GetHoudiniAssetActorOwner();
	if(HoudiniAssetActor && bIsNativeComponent)
	{
		// Manually reselect the actor - this will cause details panel to be updated and force our 
		// property changes to be picked up by the UI.
		GEditor->SelectActor(HoudiniAssetActor, true, true);

		// Notify the editor about selection change.
		GEditor->NoteSelectionChange();
	}
}


FBoxSphereBounds
UHoudiniAssetComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Bounds;

	if(AttachChildren.Num() == 0)
	{
		Bounds = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));
	}
	else
	{
		Bounds = AttachChildren[0]->CalcBounds(LocalToWorld);
	}

	for(int32 Idx = 1; Idx < AttachChildren.Num(); ++Idx)
	{
		Bounds = Bounds + AttachChildren[1]->CalcBounds(LocalToWorld);
	}

	return Bounds;
}


void
UHoudiniAssetComponent::ResetHoudiniResources()
{
	if(HapiGUID.IsValid())
	{
		// If we have a valid task GUID.
		FHoudiniEngineTaskInfo TaskInfo;

		if(FHoudiniEngine::Get().RetrieveTaskInfo(HapiGUID, TaskInfo))
		{
			FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
			HapiGUID.Invalidate();
			StopHoudiniTicking();

			if(NotificationPtr.IsValid())
			{
				TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
				if(NotificationItem.IsValid())
				{
					NotificationItem->ExpireAndFadeout();
					NotificationPtr.Reset();
				}
			}
		}
	}

	// If we have an asset.
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId) && bIsNativeComponent)
	{
		// Generate GUID for our new task.
		HapiGUID = FGuid::NewGuid();

		// Create asset deletion task object and submit it for processing.
		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetDeletion, HapiGUID);
		Task.AssetId = AssetId;
		FHoudiniEngine::Get().AddTask(Task);

		// Reset asset id
		AssetId = -1;
	}

	// Unsubscribe from Editor events.
	UnsubscribeEditorDelegates();
}


void
UHoudiniAssetComponent::UpdateRenderingInformation()
{
	// Need to send this to render thread at some point.
	MarkRenderStateDirty();

	// Update physics representation right away.
	RecreatePhysicsState();

	// Since we have new asset, we need to update bounds.
	UpdateBounds();
}


FPrimitiveSceneProxy*
UHoudiniAssetComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = nullptr;
	return Proxy;
}


void
UHoudiniAssetComponent::BeginDestroy()
{
	// Notify that the primitive has been detached from this component.
	IStreamingManager::Get().NotifyPrimitiveDetached(this);

	Super::BeginDestroy();
}


void
UHoudiniAssetComponent::FinishDestroy()
{
	Super::FinishDestroy();
}


void
UHoudiniAssetComponent::RemoveMetaDataFromEnum(UEnum* EnumObject)
{
	for(int Idx = 0; Idx < EnumObject->NumEnums(); ++Idx)
	{
		/*
		if(EnumObject->HasMetaData(TEXT("DisplayName"), Idx))
		{
			EnumObject->RemoveMetaData(TEXT("DisplayName"), Idx);
		}
		*/

		if(EnumObject->HasMetaData(TEXT("HoudiniName"), Idx))
		{
			EnumObject->RemoveMetaData(TEXT("HoudiniName"), Idx);
		}
	}
}


void
UHoudiniAssetComponent::ReplacePropertyOffset(UProperty* Property, int Offset)
{
	// We need this bit of magic in order to replace the private field.
	*(int*)((char*)&Property->RepNotifyFunc + sizeof(FName)) = Offset;
}


EHoudiniEngineProperty::Type 
UHoudiniAssetComponent::GetPropertyType(UProperty* Property) const
{
	if(UIntProperty::StaticClass() == Property->GetClass())
	{
		return EHoudiniEngineProperty::Integer;
	}
	else if(UBoolProperty::StaticClass() == Property->GetClass())
	{
		return EHoudiniEngineProperty::Boolean;
	}
	else if(UFloatProperty::StaticClass() == Property->GetClass())
	{
		return EHoudiniEngineProperty::Float;
	}
	else if(UStrProperty::StaticClass() == Property->GetClass())
	{
		return EHoudiniEngineProperty::String;
	}
	else if(UByteProperty::StaticClass() == Property->GetClass())
	{
		return EHoudiniEngineProperty::Enumeration;
	}
	else if(UObjectProperty::StaticClass() == Property->GetClass())
	{
		return EHoudiniEngineProperty::Object;
	}
	else if(UStructProperty::StaticClass() == Property->GetClass())
	{
		UStructProperty* StructProperty = Cast<UStructProperty>(Property);

		if(UHoudiniAssetComponent::ScriptStructColor == StructProperty->Struct)
		{
			return EHoudiniEngineProperty::Color;
		}
	}

	return EHoudiniEngineProperty::None;
}


UClass*
UHoudiniAssetComponent::GetPropertyType(HAPI_ParmType PropertyType, int ChoiceCount) const
{
	UClass* PropertyClass = nullptr;

	switch(PropertyType)
	{
		case HAPI_PARMTYPE_INT:
		{
			if(!ChoiceCount)
			{
				PropertyClass = UIntProperty::StaticClass();
			}
			else if(ChoiceCount >= 0)
			{
				PropertyClass = UByteProperty::StaticClass();
			}

			break;
		}

		case HAPI_PARMTYPE_STRING:
		{
			if(!ChoiceCount)
			{
				PropertyClass = UStrProperty::StaticClass();
			}
			else if(ChoiceCount >= 0)
			{
				PropertyClass = UByteProperty::StaticClass();
			}

			break;
		}

		case HAPI_PARMTYPE_FLOAT:
		{
			PropertyClass = UFloatProperty::StaticClass();
			break;
		}

		case HAPI_PARMTYPE_TOGGLE:
		{
			PropertyClass = UBoolProperty::StaticClass();
			break;
		}

		case HAPI_PARMTYPE_COLOR:
		{
			PropertyClass = UStructProperty::StaticClass();
			break;
		}

		case HAPI_PARMTYPE_PATH_NODE:
		{
			PropertyClass = UObjectProperty::StaticClass();
			break;
		}

		default:
		{
			break;
		}
	}

	return PropertyClass;
}


void
UHoudiniAssetComponent::SubscribeEditorDelegates()
{
	// Add pre and post save delegates.
	FEditorDelegates::PreSaveWorld.AddUObject(this, &UHoudiniAssetComponent::OnPreSaveWorld);
	FEditorDelegates::PostSaveWorld.AddUObject(this, &UHoudiniAssetComponent::OnPostSaveWorld);

	// Add begin and end delegates for play-in-editor.
	FEditorDelegates::BeginPIE.AddUObject(this, &UHoudiniAssetComponent::OnPIEEventBegin);
	FEditorDelegates::EndPIE.AddUObject(this, &UHoudiniAssetComponent::OnPIEEventEnd);
}


void
UHoudiniAssetComponent::UnsubscribeEditorDelegates()
{
	// Remove pre and post save delegates.
	FEditorDelegates::PreSaveWorld.RemoveUObject(this, &UHoudiniAssetComponent::OnPreSaveWorld);
	FEditorDelegates::PostSaveWorld.RemoveUObject(this, &UHoudiniAssetComponent::OnPostSaveWorld);

	// Remove begin and end delegates for play-in-editor.
	FEditorDelegates::BeginPIE.RemoveUObject(this, &UHoudiniAssetComponent::OnPIEEventBegin);
	FEditorDelegates::EndPIE.RemoveUObject(this, &UHoudiniAssetComponent::OnPIEEventEnd);
}


void
UHoudiniAssetComponent::OnPackageSaved(const FString& PackageFileName, UObject* Outer)
{
	if(!GetOuter())
	{
		return;
	}

	UObject* Package = GetOuter()->GetOuter();

	if(Outer == Package)
	{
		HOUDINI_TEST_LOG_MESSAGE( "  OnPackageSaved,                     C" );

		// We need to restore patched class information.
		RestorePatchedClassInformation();
	}
}


bool
UHoudiniAssetComponent::HasReplacedClassInformation() const
{
	return PatchedClass != nullptr;
}


void
UHoudiniAssetComponent::ComputeInputCount()
{
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		HAPI_AssetInfo AssetInfo;
		if((HAPI_RESULT_SUCCESS == HAPI_GetAssetInfo(AssetId, &AssetInfo)) && AssetInfo.hasEverCooked)
		{
			InputCount = AssetInfo.geoInputCount;
		}
	}
	else
	{
		InputCount = 0;
	}
}


void
UHoudiniAssetComponent::ClearChangedPropertiesAll()
{
	ChangedProperties.Empty();
}


void
UHoudiniAssetComponent::ClearChangedPropertiesParameters()
{
	TMap<FString, UProperty*> InputProperties;
	static const FString CategoryHoudiniInputs = TEXT("HoudiniInputs");

	for(TMap<FString, UProperty*>::TIterator Iter(ChangedProperties); Iter; ++Iter)
	{
		UProperty* Property = Iter.Value();
		const FString& Category = Property->GetMetaData(TEXT("Category"));
		if(Category == CategoryHoudiniInputs)
		{
			InputProperties.Add(Iter.Key(), Property);
		}
	}

	ChangedProperties.Empty();
	ChangedProperties = InputProperties;
}


void
UHoudiniAssetComponent::ReplaceClassInformation(const FString& ActorLabel, bool bReplace)
{
	HOUDINI_TEST_LOG_MESSAGE( "  ReplaceClassInformation,            C" );

	UClass* NewClass = nullptr;
	UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

	// If RTTI has not been previously patched, we need to do so.
	if(!PatchedClass)
	{
		// Construct unique name for this class.
		FString PatchedClassName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("%s_%s_%d"), *GetClass()->GetName(), *ActorLabel, UHoudiniAssetComponent::ComponentPatchedClassCounter));

		// Create new class instance.
		static const EObjectFlags PatchedClassFlags = RF_Public | RF_Standalone;

		// Construct the new class instance.
		NewClass = ConstructObject<UClass>(UClass::StaticClass(), GetOutermost(), FName(*PatchedClassName), PatchedClassFlags, ClassOfUHoudiniAssetComponent, true);

		// We just created a patched instance.
		UHoudiniAssetComponent::ComponentPatchedClassCounter++;

		// Use same class flags as the original class. Also make sure we remove intrinsic flag. Intrinsic flag specifies
		// that class has been generated in c++ and has no boilerplate generated by UnrealHeaderTool.
		NewClass->ClassFlags = UHoudiniAssetComponent::StaticClassFlags & ~CLASS_Intrinsic;

		// Use same class cast flags as the original class (these are used for quick casting between common types).
		NewClass->ClassCastFlags = UHoudiniAssetComponent::StaticClassCastFlags();

		// Use same class configuration name.
		NewClass->ClassConfigName = UHoudiniAssetComponent::StaticConfigName();

		// We will reuse the same constructor as nothing has really changed.
		NewClass->ClassConstructor = ClassOfUHoudiniAssetComponent->ClassConstructor;

		// Register our own reference counting registration.
		NewClass->ClassAddReferencedObjects = UHoudiniAssetComponent::AddReferencedObjects;

		// Minimum class alignment does not change.
		NewClass->MinAlignment = ClassOfUHoudiniAssetComponent->MinAlignment;

		// Properties size does not change as we use the same fixed size buffer.
		NewClass->PropertiesSize = ClassOfUHoudiniAssetComponent->PropertiesSize;

		// Set super class (we are deriving from UHoudiniAssetComponent).
		NewClass->SetSuperStruct(ClassOfUHoudiniAssetComponent->GetSuperStruct());

		// Create Class default object.
		//NewClass->ClassDefaultObject = GetClass()->ClassDefaultObject;
		NewClass->ClassDefaultObject = nullptr;

		// List of replication records.
		NewClass->ClassReps = ClassOfUHoudiniAssetComponent->ClassReps;

		// List of network relevant fields (properties and functions).
		NewClass->NetFields = ClassOfUHoudiniAssetComponent->NetFields;

		// Reference token stream used by real time garbage collector.
		NewClass->ReferenceTokenStream = ClassOfUHoudiniAssetComponent->ReferenceTokenStream;

		// This class's native functions.
		NewClass->NativeFunctionLookupTable = ClassOfUHoudiniAssetComponent->NativeFunctionLookupTable;

		// We will reuse existing properties (these are auto generated).
		NewClass->PropertyLink = ClassOfUHoudiniAssetComponent->PropertyLink;
		NewClass->Children = ClassOfUHoudiniAssetComponent->PropertyLink;

		// Store patched class.
		PatchedClass = NewClass;

		// Now that we've filled all necessary fields, patch class information.
		if(bReplace)
		{
			ReplaceClassObject(NewClass);

			// Now we need to create CDO for this newly created class.
			HOUDINI_PRIVATE_CALL_EXT_NOPARM(FPrivate_UClass_CreateDefaultObject, UClass, NewClass);

			// Now that RTTI has been patched, we need to subscribe to Editor delegates. This is necessary in order to
			// patch old RTTI information back for saving and other operations. Once save completes, we restore the 
			// patched RTTI back.
			SubscribeEditorDelegates();
		}
	}
	else
	{
		// Otherwise we need to destroy and recreate all properties.
		NewClass = GetClass();
		RemoveClassProperties(NewClass);
	}

	// Insert necessary properties.
	ReplaceClassProperties(NewClass);
}


void
UHoudiniAssetComponent::ReplaceClassObject(UClass* ClassObjectNew)
{
	HOUDINI_TEST_LOG_MESSAGE( "  ReplaceClassObject,                 C" );

	// Invoke private UObjectBase::SetClass .
	HOUDINI_PRIVATE_CALL_PARM1(FPrivate_UObjectBase_SetClass, UObjectBase, ClassObjectNew);
}


void
UHoudiniAssetComponent::RestoreOriginalClassInformation()
{
	HOUDINI_TEST_LOG_MESSAGE( "  RestoreOriginalClassInformation,    C" );

	if(!PatchedClass)
	{
		// If class information has not been patched, do nothing.
		return;
	}

	// We need to add our patched class to root in order to avoid its clean up by GC.
	PatchedClass->AddToRoot();

	// We need to restore original class information.
	ReplaceClassObject(UHoudiniAssetComponent::StaticClass());
}

void
UHoudiniAssetComponent::RestorePatchedClassInformation()
{
	HOUDINI_TEST_LOG_MESSAGE( "  RestorePatchedClassInformation,     C" );

	if(!PatchedClass)
	{
		return;
	}

	// We need to restore patched class information.
	ReplaceClassObject(PatchedClass);

	// We can put our patched class back, and remove it from root as it no longer under threat of being cleaned up by GC.
	PatchedClass->RemoveFromRoot();
}


void
UHoudiniAssetComponent::RemoveClassProperties(UClass* ClassInstance)
{
	UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();
	static const FString CategoryHoudiniAsset = TEXT("HoudiniProperties");

	UProperty* IterProperty = ClassInstance->PropertyLink;
	while(IterProperty && IterProperty != ClassOfUHoudiniAssetComponent->PropertyLink)
	{
		UProperty* Property = IterProperty;
		IterProperty = IterProperty->PropertyLinkNext;

		if(Property->HasMetaData(TEXT("Category")))
		{
			const FString& Category = Property->GetMetaData(TEXT("Category"));
			if(Category == CategoryHoudiniAsset)
			{
				Property->Next = nullptr;
				Property->PropertyLinkNext = nullptr;
			}
		}
	}

	// We need to insert back the original properties that were auto generated.
	ClassInstance->PropertyLink = ClassOfUHoudiniAssetComponent->PropertyLink;
	ClassInstance->Children = ClassOfUHoudiniAssetComponent->Children;

	// Do not need to update / remove / delete children as those will be by construction same as properties.
}


void
UHoudiniAssetComponent::AddLinkedProperty(UProperty*& PropertyFirst, UProperty*& PropertyLast, UProperty* Property)
{
	if(!PropertyFirst)
	{
		PropertyFirst = Property;
		PropertyLast = Property;
	}
	else
	{
		PropertyLast->PropertyLinkNext = Property;
		PropertyLast = Property;
	}
}


void
UHoudiniAssetComponent::AddLinkedChild(UField*& ChildFirst, UField*& ChildLast, UProperty* Property)
{
	if(!ChildFirst)
	{
		ChildFirst = Property;
		ChildLast = Property;
	}
	else
	{
		ChildLast->Next = Property;
		ChildLast = Property;
	}
}


bool
UHoudiniAssetComponent::ReplaceClassProperties(UClass* ClassInstance)
{
	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HAPI_AssetInfo AssetInfo;
	HAPI_NodeInfo NodeInfo;

	std::vector<HAPI_ParmInfo> ParmInfo;
	std::vector<int> ParmValuesIntegers;
	std::vector<float> ParmValuesFloats;
	std::vector<HAPI_StringHandle> ParmValuesStrings;
	std::vector<char> ParmName;
	std::vector<char> ParmLabel;

	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		// There's no Houdini asset, we can return. This is typically hit when component is being loaded during serialization.
		return true;
	}

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetNodeInfo(AssetInfo.nodeId, &NodeInfo), false);

	// Retrieve parameters.
	ParmInfo.resize(NodeInfo.parmCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParameters(AssetInfo.nodeId, &ParmInfo[0], 0, NodeInfo.parmCount), false);

	// Retrieve integer values for this asset.
	ParmValuesIntegers.resize(NodeInfo.parmIntValueCount);
	if(NodeInfo.parmIntValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmIntValues(AssetInfo.nodeId, &ParmValuesIntegers[0], 0, NodeInfo.parmIntValueCount), false);
	}

	// Retrieve float values for this asset.
	ParmValuesFloats.resize(NodeInfo.parmFloatValueCount);
	if(NodeInfo.parmFloatValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmFloatValues(AssetInfo.nodeId, &ParmValuesFloats[0], 0, NodeInfo.parmFloatValueCount), false);
	}

	// Retrieve string values for this asset.
	ParmValuesStrings.resize(NodeInfo.parmStringValueCount);
	if(NodeInfo.parmStringValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmStringValues(AssetInfo.nodeId, true, &ParmValuesStrings[0], 0, NodeInfo.parmStringValueCount), false);
	}

	// Reset list which keeps track of properties we have created.
	CreatedProperties.Reset();

	// We need to insert new properties and new children in the beginning of single link list.
	// This way properties and children from the original class can be reused and will not have
	// their next pointers altered.
	UProperty* PropertyFirst = nullptr;
	UProperty* PropertyLast = PropertyFirst;

	UField* ChildFirst = nullptr;
	UField* ChildLast = ChildFirst;

	uint32 ValuesOffsetStart = offsetof(UHoudiniAssetComponent, ScratchSpaceBuffer);
	uint32 ValuesOffsetEnd = ValuesOffsetStart;

	// Create properties for inputs.
	for(int InputIdx = 0; InputIdx < InputCount; ++InputIdx)
	{
		// Get input string handle.
		HAPI_StringHandle InputStringHandle;
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetInputName(AssetId, InputIdx, HAPI_INPUT_GEOMETRY, &InputStringHandle));
		if(HAPI_RESULT_SUCCESS != Result)
		{
			continue;
		}

		// Get input string from handle.
		FString InputString;
		if(!FHoudiniEngineUtils::GetHoudiniString(InputStringHandle, InputString))
		{
			continue;
		}

		// Check if this property has already changed and will be used in next cook update.
		bool bPropertyChanged = CanPropertyValueBeUpdated(HAPI_PARMTYPE_PATH_NODE, 1, InputString);

		// Create object property.
		UProperty* Property = CreatePropertyObject(ClassInstance, InputString, nullptr, ValuesOffsetEnd, bPropertyChanged);
		
		// Store necessary metadata.
		Property->SetMetaData(TEXT("Category"), TEXT("HoudiniInputs"));
		Property->SetMetaData(TEXT("HoudiniParmName"), *InputString);
		Property->SetMetaData(TEXT("DisplayName"), *InputString);

		// Store index of this input in meta data.
		Property->SetMetaData(TEXT("HoudiniInputIndex"), *FString::FromInt(InputIdx));

		// Store this property in a list of created properties.
		CreatedProperties.Add(Property);

		// Insert this newly created property in link list of properties.
		AddLinkedProperty(PropertyFirst, PropertyLast, Property);
		AddLinkedChild(ChildFirst, ChildLast, Property);
	}

	// Create properties for parameters.
	for(int ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
	{
		// Retrieve param info at this index.
		const HAPI_ParmInfo& ParmInfoIter = ParmInfo[ParamIdx];

		// If parameter is invisible, skip it.
		if(ParmInfoIter.invisible)
		{
			continue;
		}

		// Skip unsupported param types for now.
		switch(ParmInfoIter.type)
		{
			case HAPI_PARMTYPE_INT:
			case HAPI_PARMTYPE_FLOAT:
			case HAPI_PARMTYPE_TOGGLE:
			case HAPI_PARMTYPE_COLOR:
			case HAPI_PARMTYPE_STRING:
			case HAPI_PARMTYPE_PATH_NODE:
			{
				break;
			}

			default:
			{
				// Just ignore unsupported types for now.
				continue;
			}
		}

		// Retrieve length of this parameter's name.
		int32 ParmNameLength = 0;
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetStringBufLength(ParmInfoIter.nameSH, &ParmNameLength));
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving length of this parameter's name, continue onto next parameter.
			continue;
		}

		// If length of name of this parameter is zero, continue onto next parameter.
		if(!ParmNameLength)
		{
			continue;
		}

		// Retrieve name for this parameter.
		ParmName.resize(ParmNameLength);
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetString(ParmInfoIter.nameSH, &ParmName[0], ParmNameLength));
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving the name of this parameter, continue onto next parameter.
			continue;
		}

		// We need to convert name to a string Unreal understands.
		FUTF8ToTCHAR ParamNameStringConverter(&ParmName[0]);
		FName ParmNameConverted = ParamNameStringConverter.Get();

		// Create unique property name to avoid collisions.
		FString UniquePropertyName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("%s_%s"), *ClassInstance->GetName(), *ParmNameConverted.ToString()));

		// Retrieve length of this parameter's label.
		int32 ParmLabelLength = 0;
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetStringBufLength(ParmInfoIter.labelSH, &ParmLabelLength));
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving length of this parameter's label, continue onto next parameter.
			continue;
		}

		// Retrieve label for this parameter.
		ParmLabel.resize(ParmLabelLength);
		HOUDINI_CHECK_ERROR(&Result, HAPI_GetString(ParmInfoIter.labelSH, &ParmLabel[0], ParmLabelLength));
		if(HAPI_RESULT_SUCCESS != Result)
		{
			// We have encountered an error retrieving the label of this parameter, continue onto next parameter.
			continue;
		}

		// We need to convert label to a string Unreal understands.
		FUTF8ToTCHAR ParamLabelStringConverter(&ParmLabel[0]);

		UProperty* Property = nullptr;

		// Check if this property has already changed and will be used in next cook update.
		bool bPropertyChanged = CanPropertyValueBeUpdated(ParmInfoIter.type, ParmInfoIter.choiceCount, UniquePropertyName);

		switch(ParmInfoIter.type)
		{
			case HAPI_PARMTYPE_INT:
			{
				if(!ParmInfoIter.choiceCount)
				{
					Property = CreatePropertyInt(ClassInstance, UniquePropertyName, ParmInfoIter.size, &ParmValuesIntegers[ParmInfoIter.intValuesIndex], ValuesOffsetEnd, bPropertyChanged);
				}
				else if(ParmInfoIter.choiceIndex >= 0)
				{
					// This parameter is an integer choice list.

					// Get relevant choices.
					std::vector<HAPI_ParmChoiceInfo> ChoiceInfos;
					ChoiceInfos.resize(ParmInfoIter.choiceCount);
					HOUDINI_CHECK_ERROR(&Result, HAPI_GetParmChoiceLists(NodeInfo.id, &ChoiceInfos[0], ParmInfoIter.choiceIndex, ParmInfoIter.choiceCount));
					if(HAPI_RESULT_SUCCESS != Result)
					{
						continue;
					}

					// Retrieve enum value from HAPI.
					int EnumIndex = 0;
					HOUDINI_CHECK_ERROR(&Result, HAPI_GetParmIntValues(NodeInfo.id, &EnumIndex, ParmInfoIter.intValuesIndex, ParmInfoIter.size));

					// Create enum property.
					Property = CreatePropertyEnum(ClassInstance, UniquePropertyName, ChoiceInfos, EnumIndex, ValuesOffsetEnd, bPropertyChanged);
				}

				break;
			}

			case HAPI_PARMTYPE_STRING:
			{
				if(!ParmInfoIter.choiceCount)
				{
					Property = CreatePropertyString(ClassInstance, UniquePropertyName, ParmInfoIter.size, &ParmValuesStrings[ParmInfoIter.stringValuesIndex], ValuesOffsetEnd, bPropertyChanged);
				}
				else if(ParmInfoIter.choiceIndex >= 0)
				{
					// This parameter is a string choice list.

					// Get relevant choices.
					std::vector<HAPI_ParmChoiceInfo> ChoiceInfos;
					ChoiceInfos.resize(ParmInfoIter.choiceCount);
					HOUDINI_CHECK_ERROR(&Result, HAPI_GetParmChoiceLists(NodeInfo.id, &ChoiceInfos[0], ParmInfoIter.choiceIndex, ParmInfoIter.choiceCount));
					if(HAPI_RESULT_SUCCESS != Result)
					{
						continue;
					}
					
					// Retrieve enum value from HAPI.
					int EnumValue = 0;
					HOUDINI_CHECK_ERROR(&Result, HAPI_GetParmStringValues(NodeInfo.id, false, &EnumValue, ParmInfoIter.stringValuesIndex, ParmInfoIter.size));

					// Retrieve string value.
					FString EnumStringValue;
					if(!FHoudiniEngineUtils::GetHoudiniString(EnumValue, EnumStringValue))
					{
						continue;
					}

					// Create enum property.
					Property = CreatePropertyEnum(ClassInstance, UniquePropertyName, ChoiceInfos, EnumStringValue, ValuesOffsetEnd, bPropertyChanged);
				}

				break;
			}

			case HAPI_PARMTYPE_FLOAT:
			{
				Property = CreatePropertyFloat(ClassInstance, UniquePropertyName, ParmInfoIter.size, &ParmValuesFloats[ParmInfoIter.floatValuesIndex], ValuesOffsetEnd, bPropertyChanged);
				break;
			}

			case HAPI_PARMTYPE_TOGGLE:
			{
				Property = CreatePropertyToggle(ClassInstance, UniquePropertyName, ParmInfoIter.size, &ParmValuesIntegers[ParmInfoIter.intValuesIndex], ValuesOffsetEnd, bPropertyChanged);
				break;
			}

			case HAPI_PARMTYPE_COLOR:
			{
				Property = CreatePropertyColor(ClassInstance, UniquePropertyName, ParmInfoIter.size, &ParmValuesFloats[ParmInfoIter.floatValuesIndex], ValuesOffsetEnd, bPropertyChanged);
				break;
			}

			case HAPI_PARMTYPE_PATH_NODE:
			/*
			{
				FString NameString;
				if(ParmInfoIter.size && FHoudiniEngineUtils::GetHoudiniString(ParmInfoIter.typeInfoSH, NameString) && NameString == TEXT("SOP"))
				{
					Property = CreatePropertyObject(ClassInstance, UniquePropertyName, nullptr, ValuesOffsetEnd, bPropertyChanged);
				}

				break;
			}
			*/
			default:
			{
				continue;
			}
		}

		if(!Property)
		{
			// Unsupported type property - skip to next parameter.
			continue;
		}

		// Store parameter name as meta data.
		Property->SetMetaData(TEXT("HoudiniParmName"), ParamNameStringConverter.Get());

		// Use label instead of name if it is present.
		if(ParmLabelLength)
		{
			Property->SetMetaData(TEXT("DisplayName"), ParamLabelStringConverter.Get());
		}
		else
		{
			Property->SetMetaData(TEXT("DisplayName"), ParamNameStringConverter.Get());
		}

		// Set UI and physical ranges, if present.
		if(ParmInfoIter.hasUIMin)
		{
			Property->SetMetaData(TEXT("UIMin"), *FString::SanitizeFloat(ParmInfoIter.UIMin));
		}

		if(ParmInfoIter.hasUIMax)
		{
			Property->SetMetaData(TEXT("UIMax"), *FString::SanitizeFloat(ParmInfoIter.UIMax));
		}

		if(ParmInfoIter.hasMin)
		{
			Property->SetMetaData(TEXT("ClampMin"), *FString::SanitizeFloat(ParmInfoIter.min));
		}

		if(ParmInfoIter.hasMax)
		{
			Property->SetMetaData(TEXT("ClampMax"), *FString::SanitizeFloat(ParmInfoIter.max));
		}

		// Store this property in a list of created properties.
		CreatedProperties.Add(Property);

		// Insert this newly created property in link list of properties.
		AddLinkedProperty(PropertyFirst, PropertyLast, Property);
		AddLinkedChild(ChildFirst, ChildLast, Property);
	}

	UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

	// We insert properties of original component class at the end.
	if(PropertyFirst)
	{
		ClassInstance->PropertyLink = PropertyFirst;
		PropertyLast->PropertyLinkNext = ClassOfUHoudiniAssetComponent->PropertyLink;
	}
	else
	{
		ClassInstance->PropertyLink = ClassOfUHoudiniAssetComponent->PropertyLink;
	}

	// Similarly, we insert children of the original component class at the end.
	if(ChildFirst)
	{
		ClassInstance->Children = ChildFirst;
		ChildLast->Next = ClassOfUHoudiniAssetComponent->Children;
	}
	else
	{
		ClassInstance->Children = ClassOfUHoudiniAssetComponent->Children;
	}

	return true;
}


bool
UHoudiniAssetComponent::CanPropertyValueBeUpdated(HAPI_ParmType PropertyType, int ChoiceCount, const FString& PropertyName) const
{
	UProperty* const* Property = ChangedProperties.Find(PropertyName);
	bool bCanBeUpdated = true;

	if(Property)
	{
		// Get class of property we are trying to look up and see if they match.
		UClass* PropertyClass = GetPropertyType(PropertyType, ChoiceCount);
		if(PropertyClass == (*Property)->GetClass())
		{
			if(UStructProperty::StaticClass() == PropertyClass)
			{
				const UStructProperty* StructProperty = Cast<const UStructProperty>(*Property);

				// Make sure both are color properties.
				if(HAPI_PARMTYPE_COLOR == PropertyType && UHoudiniAssetComponent::ScriptStructColor == StructProperty->Struct)
				{
					// Property is of the same type, we do not need to update the value.
					bCanBeUpdated = false;
				}
			}
			else
			{
				// Property is of the same type, we do not need to update the value.
				bCanBeUpdated = false;
			}
		}
	}

	// Property is not marked as changed and its value can be updated.
	return bCanBeUpdated;
}


UField*
UHoudiniAssetComponent::CreateEnum(UClass* ClassInstance, const FString& Name, const std::vector<HAPI_ParmChoiceInfo>& Choices)
{
	// Create label for this enum.
	FString UniqueEnumName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("enum_%s"), *Name));

	// See if enum has already been created.
	UEnum* ChoiceEnum = FindObject<UEnum>(ClassInstance, *UniqueEnumName, false);
	if(!ChoiceEnum)
	{
		// Enum does not exist, we need to create a corresponding enum.
		static const EObjectFlags EnumObjectFlags = RF_Public | RF_Transient;
		ChoiceEnum = NewNamedObject<UEnum>(ClassInstance, FName(*UniqueEnumName), EnumObjectFlags);
	}

	// Remove all previous meta data.
	RemoveMetaDataFromEnum(ChoiceEnum);

	FString EnumValueName, EnumFinalValue;
	TArray<FName> EnumValues;
	TArray<FString> EnumValueNames;
	TArray<FString> EnumValueLabels;

	// Retrieve string values for these parameters.
	for(int ChoiceIdx = 0; ChoiceIdx < Choices.size(); ++ChoiceIdx)
	{
		// Process labels.
		if(FHoudiniEngineUtils::GetHoudiniString(Choices[ChoiceIdx].labelSH, EnumValueName))
		{
			EnumValueLabels.Add(EnumValueName);

			EnumFinalValue = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("%s_value_%s"), *UniqueEnumName, *EnumValueName));
			EnumValues.Add(FName(*EnumFinalValue));
		}
		else
		{
			break;
		}

		// Process names.
		if(FHoudiniEngineUtils::GetHoudiniString(Choices[ChoiceIdx].valueSH, EnumValueName))
		{
			EnumValueNames.Add(EnumValueName);
		}
		else
		{
			break;
		}
	}

	// Make sure strings have been properly retrieved.
	if(EnumValues.Num() != Choices.size())
	{
		ChoiceEnum->MarkPendingKill();
		ChoiceEnum = nullptr;
	}
	else
	{
		// Set enum entries (this will also remove previous entries).
		ChoiceEnum->SetEnums(EnumValues, false);

		// We need to set meta data in a separate pass (as meta data requires enum being initialized).
		for(int ChoiceIdx = 0; ChoiceIdx < Choices.size(); ++ChoiceIdx)
		{
			ChoiceEnum->SetMetaData(TEXT("DisplayName"), *(EnumValueLabels[ChoiceIdx]), ChoiceIdx);
			ChoiceEnum->SetMetaData(TEXT("HoudiniName"), *(EnumValueNames[ChoiceIdx]), ChoiceIdx);
		}
	}

	return ChoiceEnum;
}


UProperty*
UHoudiniAssetComponent::CreateProperty(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags, EHoudiniEngineProperty::Type PropertyType)
{
	UProperty* Property = nullptr;

	switch(PropertyType)
	{
		case EHoudiniEngineProperty::Float:
		{
			Property = CreatePropertyFloat(ClassInstance, Name, PropertyFlags);
			break;
		}

		case EHoudiniEngineProperty::Integer:
		{
			Property = CreatePropertyInt(ClassInstance, Name, PropertyFlags);
			break;
		}

		case EHoudiniEngineProperty::Boolean:
		{
			Property = CreatePropertyToggle(ClassInstance, Name, PropertyFlags);
			break;
		}

		case EHoudiniEngineProperty::String:
		{
			Property = CreatePropertyString(ClassInstance, Name, PropertyFlags);
			break;
		}

		case EHoudiniEngineProperty::Color:
		{
			Property = CreatePropertyColor(ClassInstance, Name, PropertyFlags);
			break;
		}

		case EHoudiniEngineProperty::Enumeration:
		{
			Property = CreatePropertyEnum(ClassInstance, Name, PropertyFlags);
			break;
		}

		case EHoudiniEngineProperty::Object:
		{
			Property = CreatePropertyObject(ClassInstance, Name, PropertyFlags);
			break;
		}

		default:
		{
			break;
		}
	}

	if(Property)
	{
		CreatedProperties.Add(Property);
	}

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyObject(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	UObjectProperty* Property = FindObject<UObjectProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		Property = NewNamedObject<UObjectProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = 1;

	// Set the class of this property.
	Property->PropertyClass = UStaticMesh::StaticClass();

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyObject(UClass* ClassInstance, const FString& Name, UObject* Object, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// Create property or locate existing.
	UObjectProperty* Property = Cast<UObjectProperty>(CreatePropertyObject(ClassInstance, Name, PropertyFlags));

	// We need to compute proper alignment for this type.
	UObject** Boundary = ComputeOffsetAlignmentBoundary<UObject*>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	if(bUpdateValue)
	{
		if(Object)
		{
			*Boundary = Object;
		}
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(UObject*);

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyEnum(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	UByteProperty* Property = FindObject<UByteProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		Property = NewNamedObject<UByteProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyEnum(UClass* ClassInstance, const FString& Name, const std::vector<HAPI_ParmChoiceInfo>& Choices, int32 Value, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// We need to create or reuse an enum for this property.
	UEnum* EnumType = Cast<UEnum>(CreateEnum(ClassInstance, Name, Choices));
	if(!EnumType)
	{
		return nullptr;
	}

	UByteProperty* Property = Cast<UByteProperty>(CreatePropertyEnum(ClassInstance, Name, PropertyFlags));

	// Set the enum for this property.
	Property->Enum = EnumType;

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = 1;

	// Enum uses unsigned byte.
	uint8* Boundary = ComputeOffsetAlignmentBoundary<uint8>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	if(bUpdateValue)
	{
		*Boundary = (uint8) Value;
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(uint8);

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyEnum(UClass* ClassInstance, const FString& Name, const std::vector<HAPI_ParmChoiceInfo>& Choices, const FString& ValueString, uint32& Offset, bool bUpdateValue)
{
	// Store initial offset.
	uint32 OffsetStored = Offset;

	// Create enum property with default 0 value.
	UByteProperty* Property = Cast<UByteProperty>(CreatePropertyEnum(ClassInstance, Name, Choices, 0, Offset, bUpdateValue));
	if(Property)
	{
		// Get enum for this property.
		UEnum* Enum = Property->Enum;

		// Sanitize name for comparison.
		FString ValueStringCompare = ObjectTools::SanitizeObjectName(ValueString);

		// Empty string means index 0 (comes from Houdini) and we created property with 0 index by default.
		if(!ValueString.IsEmpty())
		{
			for(int Idx = 0; Idx < Enum->NumEnums(); ++Idx)
			{
				if(Enum->HasMetaData(TEXT("HoudiniName"), Idx))
				{
					const FString& HoudiniName = Enum->GetMetaData(TEXT("HoudiniName"), Idx);

					if(!HoudiniName.Compare(ValueStringCompare, ESearchCase::IgnoreCase))
					{
						// We need to repatch the value.
						uint8* Boundary = ComputeOffsetAlignmentBoundary<uint8>(OffsetStored);
						OffsetStored = (const char*)Boundary - (const char*) this;

						// Need to patch offset for this property.
						ReplacePropertyOffset(Property, OffsetStored);

						// Write property data to which it refers by offset.
						if(bUpdateValue)
						{
							*Boundary = (uint8) Idx;
						}

						// Increment offset for next property.
						Offset = OffsetStored + sizeof(uint8);

						break;
					}
				}
			}
		}

		// We will use meta information to mark this property as one corresponding to a string choice list.
		Property->SetMetaData(TEXT("HoudiniStringChoiceList"), TEXT("1"));
	}

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyString(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	UStrProperty* Property = FindObject<UStrProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UStrProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
	}

	Property->PropertyFlags = PropertyFlags;
	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyString(UClass* ClassInstance, const FString& Name, int Count, const HAPI_StringHandle* Value, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	// Create property or locate existing.
	UProperty* Property = CreatePropertyString(ClassInstance, Name, PropertyFlags);

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	FString* Boundary = ComputeOffsetAlignmentBoundary<FString>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	for(int Index = 0; Index < Count; ++Index)
	{
		FString NameString;

		if(bUpdateValue)
		{
			if(FHoudiniEngineUtils::GetHoudiniString(*(Value + Index), NameString))
			{
				new(Boundary) FString(NameString);
			}
			else
			{
				new(Boundary) FString(TEXT("Invalid"));
			}
		}

		Offset += sizeof(FString);
	}

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyColor(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	if(!UHoudiniAssetComponent::ScriptStructColor)
	{
		UHoudiniAssetComponent::ScriptStructColor = new(UHoudiniAssetComponent::StaticClass(), TEXT("Color"), RF_Public | RF_Transient | RF_Native) 
										UScriptStruct(FPostConstructInitializeProperties(), NULL, NULL, EStructFlags(0x00000030), sizeof(FColor), ALIGNOF(FColor));

		UProperty* NewProp_A = new(UHoudiniAssetComponent::ScriptStructColor, TEXT("A"), RF_Public | RF_Transient | RF_Native) UByteProperty(CPP_PROPERTY_BASE(A, FColor), 0x0000000001000005);
		UProperty* NewProp_R = new(UHoudiniAssetComponent::ScriptStructColor, TEXT("R"), RF_Public | RF_Transient | RF_Native) UByteProperty(CPP_PROPERTY_BASE(R, FColor), 0x0000000001000005);
		UProperty* NewProp_G = new(UHoudiniAssetComponent::ScriptStructColor, TEXT("G"), RF_Public | RF_Transient | RF_Native) UByteProperty(CPP_PROPERTY_BASE(G, FColor), 0x0000000001000005);
		UProperty* NewProp_B = new(UHoudiniAssetComponent::ScriptStructColor, TEXT("B"), RF_Public | RF_Transient | RF_Native) UByteProperty(CPP_PROPERTY_BASE(B, FColor), 0x0000000001000005);

		UHoudiniAssetComponent::ScriptStructColor->StaticLink();
	}

	UStructProperty* Property = FindObject<UStructProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		Property = NewNamedObject<UStructProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
		Property->Struct = UHoudiniAssetComponent::ScriptStructColor;
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyColor(UClass* ClassInstance, const FString& Name, int Count, const float* Value, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// Color must have 3 or 4 fields.
	if(Count < 3)
	{
		return nullptr;
	}

	UProperty* Property = CreatePropertyColor(ClassInstance, Name, PropertyFlags);

	FColor ConvertedColor;

	if(Count < 4)
	{
		// Disable alpha channel if our color does not have it.
		Property->SetMetaData(TEXT("HideAlphaChannel"), TEXT("0"));

		// Convert Houdini float RGB color to Unreal int RGB color (this will set alpha to 255).
		FHoudiniEngineUtils::ConvertHoudiniColorRGB(Value, ConvertedColor);
	}
	else
	{
		// Convert Houdini float RGBA color to Unreal int RGBA color.
		FHoudiniEngineUtils::ConvertHoudiniColorRGBA(Value, ConvertedColor);
	}

	// We need to compute proper alignment for this type.
	FColor* Boundary = ComputeOffsetAlignmentBoundary<FColor>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);
	
	// Write property data to which it refers by offset.
	if(bUpdateValue)
	{
		*Boundary = ConvertedColor;
	}
	
	// Increment offset for next property.
	Offset = Offset + sizeof(FColor);

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyInt(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	UIntProperty* Property = FindObject<UIntProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UIntProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
	}

	Property->PropertyFlags = PropertyFlags;
	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("EditAnywhere"), TEXT("1"));
	Property->SetMetaData(TEXT("BlueprintReadOnly"), TEXT("1"));
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyInt(UClass* ClassInstance, const FString& Name, int Count, const int32* Value, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags =  UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	// Create property or locate existing.
	UProperty* Property = CreatePropertyInt(ClassInstance, Name, PropertyFlags);

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	int* Boundary = ComputeOffsetAlignmentBoundary<int>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	if(bUpdateValue)
	{
		for(int Index = 0; Index < Count; ++Index)
		{
			*Boundary = *(Value + Index);
		}
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(int) * Count;

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyFloat(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	UFloatProperty* Property = FindObject<UFloatProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UFloatProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
	}

	Property->PropertyFlags = PropertyFlags;
	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("EditAnywhere"), TEXT("1"));
	Property->SetMetaData(TEXT("BlueprintReadOnly"), TEXT("1"));
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyFloat(UClass* ClassInstance, const FString& Name, int Count, const float* Value, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags =  UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	// Create property or locate existing.
	UProperty* Property = CreatePropertyFloat(ClassInstance, Name, PropertyFlags);

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	float* Boundary = ComputeOffsetAlignmentBoundary<float>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	if(bUpdateValue)
	{
		for(int Index = 0; Index < Count; ++Index)
		{
			*Boundary = *(Value + Index);
		}
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(float) * Count;

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyToggle(UClass* ClassInstance, const FString& Name, uint64 PropertyFlags)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;

	UBoolProperty* Property = FindObject<UBoolProperty>(ClassInstance, *Name, false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UBoolProperty>(ClassInstance, FName(*Name), PropertyObjectFlags);
	}

	Property->SetBoolSize(sizeof(bool), true);
	Property->PropertyFlags = PropertyFlags;
	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyToggle(UClass* ClassInstance, const FString& Name, int Count, const int32* bValue, uint32& Offset, bool bUpdateValue)
{
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	// Create property or locate existing.
	UProperty* Property = CreatePropertyToggle(ClassInstance, Name, PropertyFlags);

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	bool* Boundary = ComputeOffsetAlignmentBoundary<bool>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	if(bUpdateValue)
	{
		for(int Index = 0; Index < Count; ++Index)
		{
			*Boundary = (*(bValue + Index) != 0);
		}
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(bool) * Count;

	return Property;
}


void
UHoudiniAssetComponent::SetChangedPropertyValues()
{
	HOUDINI_TEST_LOG_MESSAGE( "  SetChangedPropertyValues,           C" );

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HAPI_AssetInfo AssetInfo;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), void());

	for(TMap<FString, UProperty*>::TIterator Iter(ChangedProperties); Iter; ++Iter)
	{
		UProperty* Property = Iter.Value();

		// Retrieve offset into scratch space for this property.
		uint32 ValueOffset = Property->GetOffset_ForDebug();

		static const FString CategoryHoudiniProperties = TEXT("HoudiniProperties");
		static const FString CategoryHoudiniInputs = TEXT("HoudiniInputs");

		const FString& Category = Property->GetMetaData(TEXT("Category"));

		if(Category == CategoryHoudiniProperties)
		{
			// This is a property corresponding to parameter.
			SetChangedParameterValue(AssetInfo, Property);
		}
		else if(Category == CategoryHoudiniInputs && AssetInfo.hasEverCooked)
		{
			// This is a property corresponding to input.
			SetChangedInputValue(Property);
		}
	}
}


void
UHoudiniAssetComponent::SetChangedInputValue(UProperty* Property)
{
	HOUDINI_TEST_LOG_MESSAGE( "  SetChangedInputValue,                  C" );

	HAPI_Result Result = HAPI_RESULT_SUCCESS;

	if(!Property->HasMetaData(TEXT("HoudiniInputIndex")))
	{
		// Property corresponding to input should contain index meta information.
		check(false);
		return;
	}

	// Retrieve index meta information.
	int32 InputIndex = FCString::Atoi(*Property->GetMetaData(TEXT("HoudiniInputIndex")));

	// Make sure index is valid.
	// If we don't have a cooked asset, we can continue. Otherwise we need to make sure input index is valid.
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId) || InputIndex < InputCount)
	{
		// Make sure property is handling static meshes only.
		UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
		if(ObjectProperty && UStaticMesh::StaticClass() == ObjectProperty->PropertyClass)
		{
			// Retrieve offset into scratch space for this property and read the value (should be static mesh).
			uint32 ValueOffset = Property->GetOffset_ForDebug();
			UObject* Object = *(UObject**)((const char*) this + ValueOffset);

			// If we have valid static mesh assigned, we need to marshal it into HAPI.
			HAPI_AssetId ConnectedAssetId = -1;
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object);
			if(!StaticMesh)
			{
				// We have no static mesh assigned, reset all geometry.
				// We also do not have geometry anymore, so we need to use default geometry (Houdini logo).
				ReleaseStaticMeshResources(StaticMeshes);
				StaticMeshes.Empty();
				StaticMeshComponents.Empty();
				CreateStaticMeshHoudiniLogoResource();

				return;
			}

			if(FHoudiniEngineUtils::HapiCreateAndConnectAsset(AssetId, InputIndex, StaticMesh, ConnectedAssetId))
			{
				// We successfully created input asset, now we need to record connections for book keeping purposes.
				InputAssetIds.Add(StaticMesh, ConnectedAssetId);
			}
		}
	}
}


void
UHoudiniAssetComponent::SetChangedParameterValue(const HAPI_AssetInfo& AssetInfo, UProperty* Property)
{
	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	
	// Retrieve offset into scratch space for this property.
	uint32 ValueOffset = Property->GetOffset_ForDebug();

	// Retrieve parameter name.
	const FString& ParameterName = Property->GetMetaData(TEXT("HoudiniParmName"));
	std::wstring PropertyName(*ParameterName);
	std::string PropertyNameConverted(PropertyName.begin(), PropertyName.end());

	HAPI_ParmId ParamId;
	HAPI_ParmInfo ParamInfo;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmIdFromName(AssetInfo.nodeId, PropertyNameConverted.c_str(), &ParamId), void());
	HOUDINI_CHECK_ERROR(&Result, HAPI_GetParameters(AssetInfo.nodeId, &ParamInfo, ParamId, 1));

	if(-1 == ParamId)
	{
		// Parameter has not been found, skip this property.
		return;
	}

	if(UIntProperty::StaticClass() == Property->GetClass())
	{
		check(ParamInfo.size == Property->ArrayDim);

		std::vector<int> Values(Property->ArrayDim, 0);
		for(int Index = 0; Index < Property->ArrayDim; ++Index)
		{
			Values[Index] = *(int*)((const char*) this + ValueOffset);
			ValueOffset += sizeof(int);
		}

		HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmIntValues(AssetInfo.nodeId, &Values[0], ParamInfo.intValuesIndex, ParamInfo.size));
	}
	else if(UBoolProperty::StaticClass() == Property->GetClass())
	{
		check(ParamInfo.size == Property->ArrayDim);

		std::vector<int> Values(Property->ArrayDim, 0);
		for(int Index = 0; Index < Property->ArrayDim; ++Index)
		{
			Values[Index] = *(bool*)((const char*) this + ValueOffset);
			ValueOffset += sizeof(bool);
		}

		HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmIntValues(AssetInfo.nodeId, &Values[0], ParamInfo.intValuesIndex, ParamInfo.size));
	}
	else if(UFloatProperty::StaticClass() == Property->GetClass())
	{
		check(ParamInfo.size == Property->ArrayDim);

		std::vector<float> Values(Property->ArrayDim, 0.0f);
		for(int Index = 0; Index < Property->ArrayDim; ++Index)
		{
			Values[Index] = *(float*)((const char*) this + ValueOffset);
			ValueOffset += sizeof(float);
		}

		HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmFloatValues(AssetInfo.nodeId, &Values[0], ParamInfo.floatValuesIndex, ParamInfo.size));
	}
	else if(UStrProperty::StaticClass() == Property->GetClass())
	{
		check(ParamInfo.size == Property->ArrayDim);

		for(int Index = 0; Index < Property->ArrayDim; ++Index)
		{
			// Get string at this index.
			FString* UnrealString = (FString*) ((const char*) this + ValueOffset);
			std::string String = TCHAR_TO_UTF8(*(*UnrealString));

			HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmStringValue(AssetInfo.nodeId, String.c_str(), ParamId, Index));

			// Continue onto next offset.
			ValueOffset += sizeof(FString);
		}
	}
	else if(UByteProperty::StaticClass() == Property->GetClass())
	{
		UByteProperty* ByteProperty = Cast<UByteProperty>(Property);

		// Get index value at this offset.
		int EnumValue = (int)(*(uint8*)((const char*) this + ValueOffset));

		if(ByteProperty->HasMetaData(TEXT("HoudiniStringChoiceList")))
		{
			// This property corresponds to a string choice list.
			FText EnumText = ByteProperty->Enum->GetEnumText(EnumValue);
			std::string String = TCHAR_TO_UTF8(*EnumText.ToString());

			HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmStringValue(AssetInfo.nodeId, String.c_str(), ParamId, 0));
		}
		else
		{
			// This property corresponds to an integer choice list.
			HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmIntValues(AssetInfo.nodeId, &EnumValue, ParamInfo.intValuesIndex, ParamInfo.size));
		}
	}
	else if(UStructProperty::StaticClass() == Property->GetClass())
	{
		UStructProperty* StructProperty = Cast<UStructProperty>(Property);

		if(UHoudiniAssetComponent::ScriptStructColor == StructProperty->Struct)
		{
			// Extract color information.
			FColor UnrealColor = *(FColor*)((const char*) this + ValueOffset);
			std::vector<float> Values(4, 0.0f);

			if(StructProperty->HasMetaData(TEXT("HideAlphaChannel")))
			{
				FHoudiniEngineUtils::ConvertUnrealColorRGB(UnrealColor, &Values[0]);
				Values[3] = 1.0f;
			}
			else
			{
				FHoudiniEngineUtils::ConvertUnrealColorRGBA(UnrealColor, &Values[0]);
			}

			HOUDINI_CHECK_ERROR(&Result, HAPI_SetParmFloatValues(AssetInfo.nodeId, &Values[0], ParamInfo.floatValuesIndex, ParamInfo.size));
		}
	}
}


void
UHoudiniAssetComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	if(PropertyAboutToChange && PropertyAboutToChange->GetName() == TEXT("HoudiniAsset"))
	{
		// Memorize current Houdini Asset, since it is about to change.
		ChangedHoudiniAsset = HoudiniAsset;
		Super::PreEditChange(PropertyAboutToChange);
		return;
	}
	
	// Retrieve property category and only handle those in "HoudiniInputs".
	static const FString CategoryHoudiniInputs = TEXT("HoudiniInputs");
	const FString& Category = PropertyAboutToChange->GetMetaData(TEXT("Category"));

	if(Category == CategoryHoudiniInputs)
	{
		// See if we are changing input static mesh asset.
		UObjectProperty* ObjectProperty = Cast<UObjectProperty>(PropertyAboutToChange);
		if(ObjectProperty && UStaticMesh::StaticClass() == ObjectProperty->PropertyClass)
		{
			// Get previous static mesh.
			uint32 ValueOffset = ObjectProperty->GetOffset_ForDebug();
			UStaticMesh* Mesh = Cast<UStaticMesh>(*(UObject**)((const char*) this + ValueOffset));

			// Retrieve index meta information.
			if(Mesh && ObjectProperty->HasMetaData(TEXT("HoudiniInputIndex")))
			{
				// Get input index.
				int32 InputIndex = FCString::Atoi(*ObjectProperty->GetMetaData(TEXT("HoudiniInputIndex")));

				// Disconnect what's connected to input at this index.
				FHoudiniEngineUtils::HapiDisconnectAsset(AssetId, InputIndex);

				// We also need to delete input asset created for this static mesh.
				HAPI_AssetId const* MeshInputAssetId = InputAssetIds.Find(Mesh);
				if(MeshInputAssetId)
				{
					// Schedule this asset for deletion.
					FHoudiniEngineUtils::DestroyHoudiniAsset(*MeshInputAssetId);

					// Remove input asset from our map.
					InputAssetIds.Remove(Mesh);
				}
			}
		}
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(Before),     C" );

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If Houdini Asset is being changed and has actually changed.
	if(PropertyChangedEvent.MemberProperty->GetName() == TEXT("HoudiniAsset") && ChangedHoudiniAsset != HoudiniAsset)
	{
		if(ChangedHoudiniAsset)
		{
			// Houdini Asset has been changed, we need to reset corresponding HDA and relevant resources.
			ResetHoudiniResources();

			// We also do not have geometry anymore, so we need to use default geometry (Houdini logo).
			ReleaseStaticMeshResources(StaticMeshes);
			StaticMeshes.Empty();
			StaticMeshComponents.Empty();
			CreateStaticMeshHoudiniLogoResource();

			ChangedHoudiniAsset = nullptr;
			AssetId = -1;
		}

		// Start ticking which will update the asset. We cannot update it here as it involves potential property
		// updates. It cannot be done here because this event is fired on a property which we might change.
		StartHoudiniAssetChange();

		HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(After),      C" );
		return;
	}

	// Retrieve property which changed. Property field is a property which is being modified. MemberProperty
	// field is a property which contains the modified property (for example if modified property is a member of a
	// struct).
	UProperty* Property = PropertyChangedEvent.MemberProperty;
	UProperty* PropertyChild = PropertyChangedEvent.Property;

	// Retrieve property category.
	static const FString CategoryHoudiniProperties = TEXT("HoudiniProperties");
	static const FString CategoryHoudiniInputs = TEXT("HoudiniInputs");

	const FString& Category = Property->GetMetaData(TEXT("Category"));

	if(Category == CategoryHoudiniProperties)
	{
		// We are changing one of the Houdini properties.

		if(EPropertyChangeType::Interactive == PropertyChangedEvent.ChangeType)
		{
			if(UStructProperty::StaticClass() == Property->GetClass())
			{
				UStructProperty* StructProperty = Cast<UStructProperty>(Property);

				if(UHoudiniAssetComponent::ScriptStructColor == StructProperty->Struct)
				{
					// Ignore interactive events for color properties.
					HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(After),      C" );
					return;
				}
			}
		}
	}
	else if(Category == CategoryHoudiniInputs)
	{
		// We are changing one of the Houdini inputs.
	}
	else
	{
		HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(After),      C" );
		return;
	}

	// If this is a loaded component, we need instantiation.
	if(bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId(AssetId) && !bLoadedComponentRequiresInstantiation)
	{
		bLoadedComponentRequiresInstantiation = true;
	}

	// Mark this property as changed.
	Property->SetMetaData(TEXT("HoudiniPropertyChanged"), TEXT("1"));

	// Add changed property to the set of changes.
	ChangedProperties.Add(Property->GetName(), Property);

	// Start ticking (if we are ticking already, this will be ignored).
	StartHoudiniTicking();

	HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(After),      C" );
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();

	HOUDINI_TEST_LOG_MESSAGE( "  OnComponentCreated,                 C" );

	// Create Houdini logo static mesh and component for it.
	CreateStaticMeshHoudiniLogoResource();
}


void
UHoudiniAssetComponent::OnComponentDestroyed()
{
	HOUDINI_TEST_LOG_MESSAGE( "  OnComponentDestroyed,               C" );

	// Release all Houdini related resources.
	ResetHoudiniResources();

	// Release static mesh related resources.
	ReleaseStaticMeshResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();

	// Call super class implementation.
	Super::OnComponentDestroyed();
}


bool
UHoudiniAssetComponent::ContainsHoudiniLogoGeometry() const
{
	return bContainsHoudiniLogoGeometry;
}


void
UHoudiniAssetComponent::CreateStaticMeshHoudiniLogoResource()
{
	// Create Houdini logo static mesh and component for it.
	FHoudiniGeoPartObject HoudiniGeoPartObject;
	TMap<FHoudiniGeoPartObject, UStaticMesh*> NewStaticMeshes;
	NewStaticMeshes.Add(HoudiniGeoPartObject, FHoudiniEngine::Get().GetHoudiniLogoStaticMesh());
	CreateStaticMeshResources(NewStaticMeshes);
	bContainsHoudiniLogoGeometry = true;
}


void
UHoudiniAssetComponent::OnPreSaveWorld(uint32 SaveFlags, class UWorld* World)
{
	// We need to restore original class information.
	RestoreOriginalClassInformation();
}


void
UHoudiniAssetComponent::OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess)
{
	// We need to restore patched class information.
	RestorePatchedClassInformation();
}


void
UHoudiniAssetComponent::OnPIEEventBegin(const bool bIsSimulating)
{
	// We are now in PIE mode.
	bIsPlayModeActive = true;

	// We need to restore original class information.
	RestoreOriginalClassInformation();
}


void
UHoudiniAssetComponent::OnPIEEventEnd(const bool bIsSimulating)
{
	// We are no longer in PIE mode.
	bIsPlayModeActive = false;

	// We need to restore patched class information.
	RestorePatchedClassInformation();
}


void
UHoudiniAssetComponent::PreSave()
{
	HOUDINI_TEST_LOG_MESSAGE( "  PreSave,                            C" );

	Super::PreSave();

	// We need to restore original class information.
	RestoreOriginalClassInformation();
}


void
UHoudiniAssetComponent::PostLoad()
{
	HOUDINI_TEST_LOG_MESSAGE( "  PostLoad,                           C" );

	Super::PostLoad();

	// We loaded a component which has no asset associated with it.
	if(!HoudiniAsset)
	{
		// Set geometry to be Houdini logo geometry, since we have no other geometry.
		CreateStaticMeshHoudiniLogoResource();
		return;
	}

	if(!PatchedClass)
	{
		// Replace class information.
		ReplaceClassInformation(GetOuter()->GetName());

		// These are used to track and insert properties into new class object.
		UProperty* PropertyFirst = nullptr;
		UProperty* PropertyLast = PropertyFirst;

		UField* ChildFirst = nullptr;
		UField* ChildLast = ChildFirst;

		// We can start reconstructing properties.
		for(TArray<FHoudiniEngineSerializedProperty>::TIterator Iter = SerializedProperties.CreateIterator(); Iter; ++Iter)
		{
			FHoudiniEngineSerializedProperty& SerializedProperty = *Iter;

			// Create unique property name to avoid collisions.
			FString UniquePropertyName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("%s_%s"), *PatchedClass->GetName(), *SerializedProperty.Name));

			// Create property.
			UProperty* Property = CreateProperty(PatchedClass, UniquePropertyName, SerializedProperty.Flags, SerializedProperty.Type);

			if(!Property) continue;

			if(EHoudiniEngineProperty::Enumeration == SerializedProperty.Type)
			{
				check(SerializedProperty.Enum);

				UByteProperty* EnumProperty = Cast<UByteProperty>(Property);
				EnumProperty->Enum = SerializedProperty.Enum;
			}

			// Set rest of property flags.
			Property->ArrayDim = SerializedProperty.ArrayDim;
			Property->ElementSize = SerializedProperty.ElementSize;

			// Set any meta information.
			if(SerializedProperty.Meta.Num())
			{
				for(TMap<FName, FString>::TConstIterator ParamIter(SerializedProperty.Meta); ParamIter; ++ParamIter)
				{
					Property->SetMetaData(ParamIter.Key(), *(ParamIter.Value()));
				}
			}

			// Replace offset value for this property.
			ReplacePropertyOffset(Property, SerializedProperty.Offset);

			// Insert this newly created property in link list of properties.
			if(!PropertyFirst)
			{
				PropertyFirst = Property;
				PropertyLast = Property;
			}
			else
			{
				PropertyLast->PropertyLinkNext = Property;
				PropertyLast = Property;
			}

			// Insert this newly created property into link list of children.
			if(!ChildFirst)
			{
				ChildFirst = Property;
				ChildLast = Property;
			}
			else
			{
				ChildLast->Next = Property;
				ChildLast = Property;
			}

			// We also need to add this property to a set of changed properties.
			if(SerializedProperty.bChanged)
			{
				ChangedProperties.Add(Property->GetName(), Property);
			}
		}

		// We can remove all serialized stored properties.
		SerializedProperties.Reset();

		// And add new created properties to our newly created class.
		UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

		if(PropertyFirst)
		{
			PatchedClass->PropertyLink = PropertyFirst;
			PropertyLast->PropertyLinkNext = ClassOfUHoudiniAssetComponent->PropertyLink;
		}

		if(ChildFirst)
		{
			PatchedClass->Children = ChildFirst;
			ChildLast->Next = ClassOfUHoudiniAssetComponent->Children;
		}

		// Update properties panel.
		//UpdateEditorProperties();

		if(StaticMeshes.Num() > 0)
		{
			CreateStaticMeshResources(StaticMeshes);
		}
		else
		{
			CreateStaticMeshHoudiniLogoResource();
		}

		// Need to update rendering information.
		UpdateRenderingInformation();
	}
}


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Saving),                  C" );
	}
	else if(Ar.IsLoading())
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - Before),        C" );
	}

	Super::Serialize(Ar);

	if(Ar.IsTransacting())
	{
		// We have no support for transactions (undo system) right now.
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - After),         C" );
		return;
	}

	if(!Ar.IsSaving() && !Ar.IsLoading())
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - After),         C" );
		return;
	}

	// State of this component.
	EHoudiniAssetComponentState::Type ComponentState = EHoudiniAssetComponentState::Invalid;

	if(Ar.IsSaving())
	{
		if(FHoudiniEngineUtils::IsValidAssetId(AssetId))
		{
			// Asset has been previously instantiated.

			if(HapiGUID.IsValid())
			{
				// Asset is being re-cooked asynchronously.
				ComponentState = EHoudiniAssetComponentState::BeingCooked;
			}
			else
			{
				// We have no pending asynchronous cook requests.
				ComponentState = EHoudiniAssetComponentState::Instantiated;
			}
		}
		else
		{
			if(HoudiniAsset)
			{
				// Asset has not been instantiated and therefore must have asynchronous instantiation request in progress.
				ComponentState = EHoudiniAssetComponentState::None;
			}
			else
			{
				// Component is in invalid state (for example is a default class object).
				ComponentState = EHoudiniAssetComponentState::Invalid;
			}
		}
	}

	// Serialize component state.
	Ar << ComponentState;

	// If component is in invalid state, we can skip the rest of serialization.
	if(EHoudiniAssetComponentState::Invalid == ComponentState)
	{
		HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - After),         C" );
		return;
	}

	// Serialize asset information (package and name).
	FString HoudiniAssetPackage;
	FString HoudiniAssetName;

	if(Ar.IsSaving() && HoudiniAsset)
	{
		// Retrieve package and its name.
		UPackage* Package = Cast<UPackage>(HoudiniAsset->GetOuter());
		check(Package);
		Package->GetName(HoudiniAssetPackage);

		// Retrieve name of asset.
		HoudiniAssetName = HoudiniAsset->GetName();
	}

	// Serialize package name and object name - we will need those to reconstruct / locate the asset.
	Ar << HoudiniAssetPackage;
	Ar << HoudiniAssetName;

	// Serialize scratch space size.
	int64 ScratchSpaceSize = HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE;
	Ar << ScratchSpaceSize;

	// Serialize input count.
	Ar << InputCount;

	if(Ar.IsLoading())
	{
		// Make sure scratch space size is suitable. We need to check this because size is defined by compile time constant.
		check(ScratchSpaceSize <= HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE);
	}

	// Serialize scratch space itself.
	Ar.Serialize(&ScratchSpaceBuffer[0], ScratchSpaceSize);

	// Number of properties.
	int PropertyCount = CreatedProperties.Num();
	Ar << PropertyCount;

	// These are used to track and insert properties into new class object.
	UProperty* PropertyFirst = nullptr;
	UProperty* PropertyLast = PropertyFirst;

	UField* ChildFirst = nullptr;
	UField* ChildLast = ChildFirst;

	for(int PropertyIdx = 0; PropertyIdx < PropertyCount; ++PropertyIdx)
	{
		// Property corresponding to this index.
		UProperty* Property = nullptr;

		// Property fields we need to serialize.
		EHoudiniEngineProperty::Type PropertyType = EHoudiniEngineProperty::None;
		int32 PropertyArrayDim = 1;
		int32 PropertyElementSize = 4;
		uint64 PropertyFlags = 0u;
		int32 PropertyOffset = 0;
		FString PropertyName;
		bool PropertyChanged = false;
		TMap<FName, FString> PropertyMeta;
		TMap<FName, FString>* LookupPropertyMeta = nullptr;
		UEnum* Enum = nullptr;
		UStaticMesh* StaticMesh = nullptr;

		if(Ar.IsSaving())
		{
			// Get property at this index.
			Property = CreatedProperties[PropertyIdx];
			PropertyType = GetPropertyType(Property);
			PropertyArrayDim = Property->ArrayDim;
			PropertyElementSize = Property->ElementSize;
			PropertyFlags = Property->PropertyFlags;
			PropertyOffset = Property->GetOffset_ForDebug();

			// Retrieve name of this property.
			check(Property->HasMetaData(TEXT("HoudiniParmName")));
			PropertyName = Property->GetMetaData(TEXT("HoudiniParmName"));

			// Retrieve changed status of this property, this is optimization to avoid uploading back all properties
			// to Houdini upon loading.
			if(Property->HasMetaData(TEXT("HoudiniPropertyChanged")))
			{
				PropertyChanged = true;
			}

			if(EHoudiniEngineProperty::None == PropertyType)
			{
				// We have encountered an unsupported property type.
				check(false);
			}
		}

		// Serialize fields.
		Ar << PropertyType;
		Ar << PropertyName;
		Ar << PropertyArrayDim;
		Ar << PropertyElementSize;
		Ar << PropertyFlags;
		Ar << PropertyOffset;
		Ar << PropertyChanged;

		// Serialize any meta information for this property.
		bool PropertyMetaFound = false;

		if(Ar.IsSaving())
		{
			LookupPropertyMeta = UMetaData::GetMapForObject(Property);
			PropertyMetaFound = (LookupPropertyMeta != nullptr);
		}

		Ar << PropertyMetaFound;

		if(Ar.IsSaving() && LookupPropertyMeta)
		{
			// Save meta information associated with this property.
			Ar << *LookupPropertyMeta;
		}
		else if(Ar.IsLoading())
		{
			// Load meta information for this property.
			Ar << PropertyMeta;

			// Make sure changed meta flag does not get serialized back.
			PropertyMeta.Remove(TEXT("HoudiniPropertyChanged"));
		}

		switch(PropertyType)
		{
			// Serialization of String properties.
			case EHoudiniEngineProperty::String:
			{
				// If it is a string property, we need to reconstruct string in case of loading.
				FString* UnrealString = (FString*) ((const char*) this + PropertyOffset);

				if(Ar.IsSaving())
				{
					Ar << *UnrealString;
				}
				else if(Ar.IsLoading())
				{
					FString StoredString;
					Ar << StoredString;
					new(UnrealString) FString(StoredString);
				}

				break;
			}

			// Serialization of Enumeration properties (used for choice lists).
			case EHoudiniEngineProperty::Enumeration:
			{
				if(Ar.IsSaving())
				{
					UByteProperty* EnumProperty = Cast<UByteProperty>(Property);
					Enum = EnumProperty->Enum;

					// Store flags of the enum object and patch them to flags we need.
					EObjectFlags EnumFlags = Enum->GetFlags();
					Enum->ClearFlags(RF_AllFlags);
					Enum->SetFlags(RF_Public);

					// Store the enum object.
					Ar << Enum;

					// Restore enum flags.
					Enum->ClearFlags(RF_AllFlags);
					Enum->SetFlags(EnumFlags);

					// Meta properties for enum entries are not serialized automatically, we need to save them manually.
					int EnumEntries = Enum->NumEnums() - 1;
					Ar << EnumEntries;

					// Store all entry names.
					for(int Idx = 0; Idx < EnumEntries; ++Idx)
					{
						// Store entry name.
						FString EnumEntryName = Enum->GetEnumName(Idx);
						Ar << EnumEntryName;
					}

					for(int Idx = 0; Idx < EnumEntries; ++Idx)
					{
						// Store entry meta information.
						{
							FString EnumEntryMetaDisplayName = Enum->GetMetaData(TEXT("DisplayName"), Idx);
							Ar << EnumEntryMetaDisplayName;
						}
						{
							FString EnumEntryMetaHoudiniName = Enum->GetMetaData(TEXT("HoudiniName"), Idx);
							Ar << EnumEntryMetaHoudiniName;
						}
					}
				}
				else if(Ar.IsLoading())
				{
					// Load enum object.
					Ar << Enum;

					// Meta properties for enum entries are not serialized automatically, we need to load them manually.
					int EnumEntries = 0;
					Ar << EnumEntries;

					TArray<FName> EnumValues;

					// Load enum entries.
					for(int Idx = 0; Idx < EnumEntries; ++Idx)
					{
						// Get entry name;
						FString EnumEntryName;
						Ar << EnumEntryName;

						EnumValues.Add(FName(*EnumEntryName));
					}

					// Set entries for this enum.
					Enum->SetEnums(EnumValues, false);

					// Load meta information for each entry.
					for(int Idx = 0; Idx < EnumEntries; ++Idx)
					{
						{
							FString EnumEntryMetaDisplayName;
							Ar << EnumEntryMetaDisplayName;

							Enum->SetMetaData(TEXT("DisplayName"), *EnumEntryMetaDisplayName, Idx);
						}
						{
							FString EnumEntryMetaHoudiniName;
							Ar << EnumEntryMetaHoudiniName;

							Enum->SetMetaData(TEXT("HoudiniName"), *EnumEntryMetaHoudiniName, Idx);
						}
					}
				}

				break;
			}

			// Serialization of Object properties (used for inputs).
			case EHoudiniEngineProperty::Object:
			{
				bool bInputProperty = false;
				bool bMeshAssigned = false;

				FString MeshPackage;
				FString MeshName;

				if(Ar.IsSaving())
				{
					bInputProperty = Property->HasMetaData(TEXT("HoudiniInputIndex"));
				}

				// Serialize the flag that specifies whether this object property is an input.
				Ar << bInputProperty;

				if(Ar.IsSaving())
				{
					// Get Mesh for this property.
					UObject* ScratchSpaceObject = *(UObject**)((const char*) this + PropertyOffset);
					if(ScratchSpaceObject)
					{
						StaticMesh = Cast<UStaticMesh>(ScratchSpaceObject);

						// If mesh is set.
						if(StaticMesh)
						{
							bMeshAssigned = true;

							// Retrieve package of mesh.
							UPackage* Package = Cast<UPackage>(StaticMesh->GetOuter());
							Package->GetName(MeshPackage);

							// Retrieve name of mesh.
							MeshName = StaticMesh->GetName();
						}
					}
				}

				Ar << bMeshAssigned;
				if(bMeshAssigned)
				{
					Ar << MeshPackage;
					Ar << MeshName;
				}

				if(Ar.IsLoading())
				{
					UObject** ScratchSpaceObject = (UObject**)((const char*) this + PropertyOffset);

					if(bMeshAssigned)
					{
						// Attempt to locate package which contains referenced static mesh.
						UPackage* Package = FindPackage(NULL, *MeshPackage);
						if(!Package)
						{
							// Package was not loaded previously, we will try to load it.
							Package = PackageTools::LoadPackage(MeshPackage);
						}

						if(Package)
						{
							StaticMesh = Cast<UStaticMesh>(StaticFindObject(UStaticMesh::StaticClass(), Package, *MeshName, true));
						}
						else
						{
							// Package does not exist - we cannot locate necessary mesh.
						}
					}
					else
					{
						// We have no mesh assigned for this property.
					}

					// Set pointer in scratch space buffer.
					*ScratchSpaceObject = StaticMesh;
				}

				break;
			}

			default:
			{
				break;
			}
		}

		// At this point if we are loading, we can construct intermediate object.
		if(Ar.IsLoading())
		{
			FHoudiniEngineSerializedProperty SerializedProperty(PropertyType, PropertyName, PropertyFlags,
																PropertyArrayDim, PropertyElementSize, PropertyOffset, 
																PropertyChanged);
			if(PropertyMeta.Num())
			{
				SerializedProperty.Meta = PropertyMeta;
			}

			// If this is enum property, store corresponding enum.
			if(EHoudiniEngineProperty::Enumeration == PropertyType)
			{
				check(Enum);
				SerializedProperty.Enum = Enum;
			}

			// Store property in a list.
			SerializedProperties.Add(SerializedProperty);
		}
	}

	// Serialize geometry - generated raw static meshes.
	int32 NumStaticMeshes = StaticMeshes.Num();

	// Serialize geos only if they are not Houdini logo.
	if(Ar.IsSaving() && bContainsHoudiniLogoGeometry)
	{
		NumStaticMeshes = 0;
	}

	// Serialize number of geos.
	Ar << NumStaticMeshes;

	if(NumStaticMeshes > 0)
	{
		if(Ar.IsSaving())
		{
			for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshes); Iter; ++Iter)
			{
				FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
				UStaticMesh* StaticMesh = Iter.Value();

				// Store the object geo part information.
				HoudiniGeoPartObject.Serialize(Ar);

				// Serialize raw mesh.
				FHoudiniEngineUtils::SaveRawStaticMesh(StaticMesh, Ar);
			}
		}
		else if(Ar.IsLoading())
		{
			for(int StaticMeshIdx = 0; StaticMeshIdx < NumStaticMeshes; ++StaticMeshIdx)
			{
				FHoudiniGeoPartObject HoudiniGeoPartObject;

				// Get object geo part information.
				HoudiniGeoPartObject.Serialize(Ar);

				// Load the raw mesh.
				UStaticMesh* StaticMesh = FHoudiniEngineUtils::LoadRawStaticMesh(HoudiniAsset, nullptr, StaticMeshIdx, Ar);
				if(StaticMesh)
				{
					StaticMeshes.Add(HoudiniGeoPartObject, StaticMesh);
				}
			}
		}
	}

	if(Ar.IsLoading())
	{
		// We need to recompute bounding volume.
		//ComputeComponentBoundingVolume();
	}

	if(Ar.IsLoading() && bIsNativeComponent)
	{
		// This component has been loaded.
		bLoadedComponent = true;

		// We need to locate corresponding package and load it if it is not loaded.
		UPackage* Package = FindPackage(NULL, *HoudiniAssetPackage);
		if(!Package)
		{
			// Package was not loaded previously, we will try to load it.
			Package = PackageTools::LoadPackage(HoudiniAssetPackage);
		}

		if(!Package)
		{
			// Package does not exist - this is a problem, we cannot continue. Component will have no asset.
			return;
		}

		// At this point we can locate the asset, since package exists.
		UHoudiniAsset* HoudiniAssetLookup = Cast<UHoudiniAsset>(StaticFindObject(UHoudiniAsset::StaticClass(), Package, *HoudiniAssetName, true));
		if(HoudiniAssetLookup)
		{
			// Set asset for this component. This will trigger asynchronous instantiation.
			SetHoudiniAsset(HoudiniAssetLookup);
		}
	}

	HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - After),         C" );
}

#undef HOUDINI_TEST_LOG_MESSAGE
