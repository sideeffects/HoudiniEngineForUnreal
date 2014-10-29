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


bool
UHoudiniAssetComponent::bDisplayEngineNotInitialized = true;


UHoudiniAssetComponent::UHoudiniAssetComponent(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	HoudiniAsset(nullptr),
	ChangedHoudiniAsset(nullptr),
	AssetId(-1),
	HapiNotificationStarted(0.0),
	bContainsHoudiniLogoGeometry(false),
	bIsNativeComponent(false),
	bIsPreviewComponent(false),
	bLoadedComponent(false),
	bLoadedComponentRequiresInstantiation(false),
	bInstantiated(false),
	bIsPlayModeActive(false),
	bParametersChanged(false)
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
}


UHoudiniAssetComponent::~UHoudiniAssetComponent()
{

}


void
UHoudiniAssetComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InThis);

	if(HoudiniAssetComponent && !HoudiniAssetComponent->IsPendingKill())
	{
		// Add references for all parameters.
		for(TMap<uint32, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
			Collector.AddReferencedObject(HoudiniAssetParameter, InThis);
		}

		// Add references to all inputs.
		for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(HoudiniAssetComponent->Inputs); IterInputs; ++IterInputs)
		{
			UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
			Collector.AddReferencedObject(HoudiniAssetInput, InThis);
		}

		// Add references to all instance inputs.
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator Iter(HoudiniAssetComponent->InstanceInputs); Iter; ++Iter)
		{
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Iter.Value();
			Collector.AddReferencedObject(HoudiniAssetInstanceInput, InThis);
		}

		// Add references to all static meshes and corresponding geo parts.
		for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(HoudiniAssetComponent->StaticMeshes); Iter; ++Iter)
		{
			UStaticMesh* StaticMesh = Iter.Value();
			if(StaticMesh)
			{
				Collector.AddReferencedObject(StaticMesh, InThis);
			}
		}

		// Add references to all static meshes and their static mesh components.
		for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(HoudiniAssetComponent->StaticMeshComponents); Iter; ++Iter)
		{
			UStaticMesh* StaticMesh = Iter.Key();
			UStaticMeshComponent* StaticMeshComponent = Iter.Value();

			Collector.AddReferencedObject(StaticMesh, InThis);
			Collector.AddReferencedObject(StaticMeshComponent, InThis);
		}

		// Retrieve asset associated with this component.
		//UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
		//if(HoudiniAsset)
		//{
		//	// Manually add a reference to Houdini asset from this component.
		//	Collector.AddReferencedObject(HoudiniAsset, InThis);
		//}
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

	if(!bIsNativeComponent)
	{
		return;
	}

	// Set Houdini logo to be default geometry.
	ReleaseObjectGeoPartResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();
	CreateStaticMeshHoudiniLogoResource();

	// Release all instanced mesh resources.
	//MarkAllInstancersUnused();
	//ClearAllUnusedInstancers();

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
UHoudiniAssetComponent::CreateObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	// Reset Houdini logo flag.
	bContainsHoudiniLogoGeometry = false;

	// Reset array used for static mesh preview.
	PreviewStaticMeshes.Empty();

	// We need to store instancers as they need to be processed after all other meshes.
	TArray<FHoudiniGeoPartObject> FoundInstancers;
	TArray<FHoudiniGeoPartObject> FoundCurves;

	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshMap); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(HoudiniGeoPartObject.IsInstancer())
		{
			// This geo part is an instancer and has no mesh assigned.
			check(!StaticMesh);
			FoundInstancers.Add(HoudiniGeoPartObject);
		}
		else if(HoudiniGeoPartObject.IsCurve())
		{
			// This geo part is a curve and has no mesh assigned.
			check(!StaticMesh);
			FoundCurves.Add(HoudiniGeoPartObject);
		}
		else if(HoudiniGeoPartObject.IsVisible())
		{
			// This geo part is visible and not an instancer and must have static mesh assigned.
			check(StaticMesh);
			
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
				
			// Add static mesh to preview list.
			PreviewStaticMeshes.Add(StaticMesh);
		}
	}

	// Skip self assignment.
	if(&StaticMeshes != &StaticMeshMap)
	{
		StaticMeshes = StaticMeshMap;
	}

	if(FHoudiniEngineUtils::IsHoudiniAssetValid(AssetId))
	{
		// Create necessary instance inputs.
		CreateInstanceInputs(FoundInstancers);

		// Process curves.
		TMap<FHoudiniGeoPartObject, USplineComponent*> NewSplineComponents;
		for(TArray<FHoudiniGeoPartObject>::TIterator Iter(FoundCurves); Iter; ++Iter)
		{
			const FHoudiniGeoPartObject& HoudiniGeoPartObject = *Iter;
			AddAttributeCurve(HoudiniGeoPartObject, NewSplineComponents);
		}

		// Remove unused spline components.
		ClearAllCurves();
		SplineComponents = NewSplineComponents;
	}
}


void
UHoudiniAssetComponent::ReleaseObjectGeoPartResources(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshMap); Iter; ++Iter)
	{
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

						// Create parameters and inputs.
						CreateParameters();
						CreateInputs();

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
								ReleaseObjectGeoPartResources(StaticMeshes);

								// Set meshes and create new components for those meshes that do not have them.
								CreateObjectGeoPartResources(NewStaticMeshes);
							}
						}

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
						CreateInputs();

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

	//if(!HapiGUID.IsValid() && (bInstantiated || (ChangedProperties.Num() > 0)))
	if(!HapiGUID.IsValid() && (bInstantiated || bParametersChanged))
	{
		// If we are not cooking and we have property changes queued up.

		// Grab current time for delayed notification.
		HapiNotificationStarted = FPlatformTime::Seconds();

		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		// This component has been loaded and requires instantiation.
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
			/*
			// We need to set all property values.
			SetChangedPropertyValues();

			HAPI_AssetInfo AssetInfo;
			if((HAPI_RESULT_SUCCESS == HAPI_GetAssetInfo(AssetId, &AssetInfo)) && AssetInfo.hasEverCooked)
			{
				// Remove all processed parameters.
				ChangedProperties.Empty();
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
			*/

			// Upload changed parameters back to HAPI.
			UploadChangedParameters();

			// Create asset cooking task object and submit it for processing.
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
		Bounds = Bounds + AttachChildren[Idx]->CalcBounds(LocalToWorld);
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
UHoudiniAssetComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	if(!PropertyAboutToChange)
	{
		Super::PreEditChange(PropertyAboutToChange);
		return;
	}

	if(PropertyAboutToChange && PropertyAboutToChange->GetName() == TEXT("HoudiniAsset"))
	{
		// Memorize current Houdini Asset, since it is about to change.
		ChangedHoudiniAsset = HoudiniAsset;
		Super::PreEditChange(PropertyAboutToChange);
		return;
	}

	Super::PreEditChange(PropertyAboutToChange);
}


void
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	HOUDINI_TEST_LOG_MESSAGE( "  PostEditChangeProperty(Before),     C" );

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!bIsNativeComponent)
	{
		return;
	}

	// If Houdini Asset is being changed and has actually changed.
	if(PropertyChangedEvent.MemberProperty->GetName() == TEXT("HoudiniAsset") && ChangedHoudiniAsset != HoudiniAsset)
	{
		if(ChangedHoudiniAsset)
		{
			// Houdini Asset has been changed, we need to reset corresponding HDA and relevant resources.
			ResetHoudiniResources();

			// Clear all created parameters.
			ClearParameters();

			// Clear all inputs.
			ClearInputs();

			// Clear all instance inputs.
			ClearInstanceInputs();

			// We also do not have geometry anymore, so we need to use default geometry (Houdini logo).
			ReleaseObjectGeoPartResources(StaticMeshes);
			StaticMeshes.Empty();
			StaticMeshComponents.Empty();
			CreateStaticMeshHoudiniLogoResource();

			ChangedHoudiniAsset = nullptr;
			AssetId = -1;
		}

		// Start ticking which will update the asset. We cannot update it here as it involves potential property
		// updates. It cannot be done here because this event is fired on a property which we might change.
		StartHoudiniAssetChange();
	}

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
	ReleaseObjectGeoPartResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();

	// Release all curve related resources.
	ClearAllCurves();

	// Destroy all parameters.
	ClearParameters();

	// Destroy all inputs.
	ClearInputs();

	// Destroy all instance inputs.
	ClearInstanceInputs();
}


bool
UHoudiniAssetComponent::ContainsHoudiniLogoGeometry() const
{
	return bContainsHoudiniLogoGeometry;
}


void
UHoudiniAssetComponent::CreateStaticMeshHoudiniLogoResource()
{
	if(!bIsNativeComponent)
	{
		return;
	}

	// Create Houdini logo static mesh and component for it.
	FHoudiniGeoPartObject HoudiniGeoPartObject;
	TMap<FHoudiniGeoPartObject, UStaticMesh*> NewStaticMeshes;
	NewStaticMeshes.Add(HoudiniGeoPartObject, FHoudiniEngine::Get().GetHoudiniLogoStaticMesh());
	CreateObjectGeoPartResources(NewStaticMeshes);
	bContainsHoudiniLogoGeometry = true;
}


void
UHoudiniAssetComponent::OnPreSaveWorld(uint32 SaveFlags, class UWorld* World)
{

}


void
UHoudiniAssetComponent::OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess)
{

}


void
UHoudiniAssetComponent::OnPIEEventBegin(const bool bIsSimulating)
{
	// We are now in PIE mode.
	bIsPlayModeActive = true;
}


void
UHoudiniAssetComponent::OnPIEEventEnd(const bool bIsSimulating)
{
	// We are no longer in PIE mode.
	bIsPlayModeActive = false;
}


void
UHoudiniAssetComponent::PreSave()
{
	HOUDINI_TEST_LOG_MESSAGE( "  PreSave,                            C" );

	Super::PreSave();
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

	/*
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

			// Restore instancing inputs.
			if(EHoudiniEngineProperty::Object == SerializedProperty.Type)
			{
				static const FString CategoryHoudiniInstancedInputs = TEXT("HoudiniInstancedInputs");
				const FString& Category = Property->GetMetaData(TEXT("Category"));

				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
				if(ObjectProperty && (Category == CategoryHoudiniInstancedInputs))
				{
					check(ObjectProperty->HasMetaData(TEXT("HoudiniParmName")));

					// Locate instancer.
					FHoudiniEngineInstancer* const* FoundHoudiniEngineInstancer = InstancerPropertyNames.Find(ObjectProperty->GetMetaData(TEXT("HoudiniParmName")));
					if(FoundHoudiniEngineInstancer)
					{
						FHoudiniEngineInstancer* HoudiniEngineInstancer = *FoundHoudiniEngineInstancer;
						HoudiniEngineInstancer->SetObjectProperty(ObjectProperty);
						InstancerProperties.Add(ObjectProperty, HoudiniEngineInstancer);
					}
					else
					{
						check(false);
					}
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

		// We can reset insntacing input name mapping.
		InstancerPropertyNames.Empty();

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

		// Create instanced static mesh resources.
		CreateInstancedStaticMeshResources();

		// Need to update rendering information.
		UpdateRenderingInformation();
	}
	*/
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

	// Serialize parameters.
	SerializeParameters(Ar);

	// Serialize inputs.
	{
		int32 InputCount = Inputs.Num();
		Ar << InputCount;
	}

	// Serialize static meshes / geometry.
	{
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
					UStaticMesh* LoadedStaticMesh = nullptr;
					if(!HoudiniGeoPartObject.bIsInstancer)
					{
						LoadedStaticMesh = FHoudiniEngineUtils::LoadRawStaticMesh(HoudiniAsset, nullptr, StaticMeshIdx, Ar);
					}

					StaticMeshes.Add(HoudiniGeoPartObject, LoadedStaticMesh);
				}
			}
		}
	}

	// Serialize instancing data.
	{
		int32 NumInstancers = InstanceInputs.Num();
		Ar << NumInstancers;

		/*
		if(NumInstancers > 0)
		{
			InstancerPropertyNames.Empty();

			if(Ar.IsSaving())
			{
				for(TMap<FHoudiniGeoPartObject, FHoudiniEngineInstancer*>::TIterator IterInstancer(Instancers); IterInstancer; ++IterInstancer)
				{
					FHoudiniGeoPartObject& HoudiniGeoPartObject = IterInstancer.Key();
					FHoudiniEngineInstancer* HoudiniEngineInstancer = IterInstancer.Value();

					// Store the object geo part information.
					HoudiniGeoPartObject.Serialize(Ar);

					// Store instancer information.
					HoudiniEngineInstancer->Serialize(Ar);
				}
			}
			else if(Ar.IsLoading())
			{
				for(int32 InstancerIdx = 0; InstancerIdx < NumInstancers; ++InstancerIdx)
				{
					FHoudiniGeoPartObject HoudiniGeoPartObject;
					FHoudiniEngineInstancer* HoudiniEngineInstancer = nullptr;

					// Get object geo part information.
					HoudiniGeoPartObject.Serialize(Ar);

					// Look up original mesh.
					UStaticMesh* const* FoundOriginalMesh = StaticMeshes.Find(HoudiniGeoPartObject);
					UStaticMesh* OriginalMesh = nullptr;
					if(FoundOriginalMesh)
					{
						OriginalMesh = *FoundOriginalMesh;
					}

					// Create instancer and get instancer information.
					HoudiniEngineInstancer = new FHoudiniEngineInstancer(OriginalMesh);
					HoudiniEngineInstancer->Serialize(Ar);

					if(!OriginalMesh)
					{
						delete HoudiniEngineInstancer;
						continue;
					}

					// Store instancer geo part mapping.
					Instancers.Add(HoudiniGeoPartObject, HoudiniEngineInstancer);

					// Store property name mapping for this instancer.
					InstancerPropertyNames.Add(HoudiniEngineInstancer->ObjectPropertyName, HoudiniEngineInstancer);
				}
			}
		}
		*/
	}

	if(Ar.IsLoading())
	{
		// We need to recompute bounding volume.
		//ComputeComponentBoundingVolume();
	}


	/*
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
	*/

	HOUDINI_TEST_LOG_MESSAGE( "  Serialize(Loading - After),         C" );
}


bool
UHoudiniAssetComponent::LocateStaticMeshes(const FString& ObjectName, TMultiMap<FString, FHoudiniGeoPartObject>& InOutObjectsToInstance, bool bSubstring) const
{
	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TConstIterator Iter(StaticMeshes); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(StaticMesh && ObjectName.Len() > 0)
		{
			if(bSubstring && ObjectName.Len() >= HoudiniGeoPartObject.ObjectName.Len())
			{
				int32 Index = ObjectName.Find(*HoudiniGeoPartObject.ObjectName, ESearchCase::IgnoreCase, ESearchDir::FromEnd, INDEX_NONE);
				if((Index != -1) && (Index + HoudiniGeoPartObject.ObjectName.Len() == ObjectName.Len()))
				{
					InOutObjectsToInstance.Add(ObjectName, HoudiniGeoPartObject);
				}
			}
			else if(HoudiniGeoPartObject.ObjectName.Equals(ObjectName))
			{
				InOutObjectsToInstance.Add(ObjectName, HoudiniGeoPartObject);
			}
		}
	}

	return InOutObjectsToInstance.Num() > 0;
}


bool
UHoudiniAssetComponent::LocateStaticMeshes(int ObjectToInstanceId, TArray<FHoudiniGeoPartObject>& InOutObjectsToInstance) const
{
	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TConstIterator Iter(StaticMeshes); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
		UStaticMesh* StaticMesh = Iter.Value();

		if(StaticMesh && HoudiniGeoPartObject.ObjectId == ObjectToInstanceId)
		{
			InOutObjectsToInstance.Add(HoudiniGeoPartObject);
		}
	}

	return InOutObjectsToInstance.Num() > 0;
}


bool
UHoudiniAssetComponent::AddAttributeCurve(const FHoudiniGeoPartObject& HoudiniGeoPartObject, TMap<FHoudiniGeoPartObject, USplineComponent*>& NewSplineComponents)
{
	HAPI_GeoInfo GeoInfo;
	HAPI_GetGeoInfo(HoudiniGeoPartObject.AssetId, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, &GeoInfo);

	HAPI_NodeInfo NodeInfo;
	HAPI_GetNodeInfo(GeoInfo.nodeId, &NodeInfo);

	if(!NodeInfo.parmCount)
	{
		return false;
	}

	static const std::string ParamCoords = "coords";
	static const std::string ParamType = "type";
	static const std::string ParamMethod = "method";

	FString CurveCoords;
	int CurveTypeValue = 2;
	int CurveMethodValue = 0;

	if(FHoudiniEngineUtils::HapiGetParameterDataAsString(GeoInfo.nodeId, ParamCoords, TEXT(""), CurveCoords) &&
	   FHoudiniEngineUtils::HapiGetParameterDataAsInteger(GeoInfo.nodeId, ParamType, 2, CurveTypeValue) &&
	   FHoudiniEngineUtils::HapiGetParameterDataAsInteger(GeoInfo.nodeId, ParamMethod, 0, CurveMethodValue))
	{
		// Check if we support method, 0 - cv/tangents, 1 - breakpoints/autocompute.
		if(0 != CurveMethodValue && 1 != CurveMethodValue)
		{
			return false;
		}

		// Process coords string and extract positions.
		TArray<FVector> CurvePoints;
		FHoudiniEngineUtils::ExtractStringPositions(CurveCoords, CurvePoints);

		/*
		if(CurvePoints.Num() < 6 && (CurvePoints.Num() % 2 == 0))
		{
			return false;
		}
		*/

		// See if spline component already has been created for this curve.
		USplineComponent* const* FoundSplineComponent = SplineComponents.Find(HoudiniGeoPartObject);
		USplineComponent* SplineComponent = nullptr;

		if(FoundSplineComponent)
		{
			// Spline component has been previously created.
			SplineComponent = *FoundSplineComponent;

			// We can remove this spline component from current map.
			SplineComponents.Remove(HoudiniGeoPartObject);
		}
		else
		{
			// We need to create a new spline component.
			SplineComponent = ConstructObject<USplineComponent>(USplineComponent::StaticClass(), GetOwner(), NAME_None, RF_Transient);

			// Add to map of components.
			NewSplineComponents.Add(HoudiniGeoPartObject, SplineComponent);

			SplineComponent->AttachTo(this);
			SplineComponent->RegisterComponent();
			SplineComponent->SetVisibility(true);
			SplineComponent->bAllowSplineEditingPerInstance = true;
			SplineComponent->bStationaryEndpoints = true;
			SplineComponent->ReparamStepsPerSegment = 25;
		}

		// Transform the component.
		SplineComponent->SetRelativeTransform(FTransform(HoudiniGeoPartObject.TransformMatrix));

		if(0 == CurveMethodValue)
		{
			// This is CV mode, we get tangents together with points.

			// Get spline info for this spline.
			FInterpCurveVector& SplineInfo = SplineComponent->SplineInfo;
			SplineInfo.Points.Reset(CurvePoints.Num() / 3);

			SplineComponent->SetSplineLocalPoints(CurvePoints);
		}
		else if(1 == CurveMethodValue)
		{
			// This is breakpoint mode, tangents need to be autocomputed.
			SplineComponent->SetSplineLocalPoints(CurvePoints);
		}
		/*
		// Set points for this component.
		{
			float InputKey = 0.0f;
			for(int32 PointIndex = 0; PointIndex < CurvePoints.Num(); PointIndex += 3)
			{
				// Retrieve Arriving vector, position and Leaving vector.
				const FVector& VectorArriving = CurvePoints[PointIndex + 0];
				const FVector& VectorPosition = CurvePoints[PointIndex + 1];
				const FVector& VectorLeaving = CurvePoints[PointIndex + 2];

				// Add point, we need to un-project to local frame.
				int32 Idx = SplineInfo.AddPoint(InputKey, VectorPosition);
				SplineInfo.Points[Idx].InterpMode = CIM_CurveUser;
				SplineInfo.Points[Idx].ArriveTangent = VectorPosition - VectorArriving;
				SplineInfo.Points[Idx].LeaveTangent = VectorLeaving - VectorPosition;

				if(PointIndex == 0)
				{
					SplineInfo.Points[Idx].ArriveTangent = SplineInfo.Points[Idx].LeaveTangent;
				}
				else if(PointIndex + 3 >= CurvePoints.Num())
				{
					SplineInfo.Points[Idx].LeaveTangent = SplineInfo.Points[Idx].ArriveTangent;
				}

				InputKey += 1.0f;
			}

			SplineComponent->UpdateSpline();
		}
		*/

		// Insert this spline component into new map.
		NewSplineComponents.Add(HoudiniGeoPartObject, SplineComponent);

		return true;
	}

	return false;
}


void
UHoudiniAssetComponent::ClearAllCurves()
{
	for(TMap<FHoudiniGeoPartObject, USplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		USplineComponent* SplineComponent = Iter.Value();

		SplineComponent->DetachFromParent();
		SplineComponent->UnregisterComponent();
		SplineComponent->DestroyComponent();
	}
}


UHoudiniAssetParameter*
UHoudiniAssetComponent::FindHoudiniAssetParameter(uint32 HashValue) const
{
	UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

	// See if parameter exists.
	UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = Parameters.Find(HashValue);

	if(FoundHoudiniAssetParameter)
	{
		HoudiniAssetParameter = *FoundHoudiniAssetParameter;
	}

	return HoudiniAssetParameter;
}


UHoudiniAssetParameter*
UHoudiniAssetComponent::FindHoudiniAssetParameter(HAPI_NodeId NodeId, HAPI_ParmId ParmId) const
{
	uint32 HashValue = UHoudiniAssetParameter::GetParameterHash(NodeId, ParmId);
	UHoudiniAssetParameter* HoudiniAssetParameter = FindHoudiniAssetParameter(HashValue);
	return HoudiniAssetParameter;
}


void
UHoudiniAssetComponent::RemoveHoudiniAssetParameter(HAPI_NodeId NodeId, HAPI_ParmId ParmId)
{
	uint32 ValueHash = UHoudiniAssetParameter::GetParameterHash(NodeId, ParmId);
	RemoveHoudiniAssetParameter(ValueHash);
}


void
UHoudiniAssetComponent::RemoveHoudiniAssetParameter(uint32 HashValue)
{
	Parameters.Remove(HashValue);
}


bool
UHoudiniAssetComponent::CreateParameters()
{
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		// There's no Houdini asset, we can return.
		return true;
	}

	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

	// Map of newly created and reused parameters.
	TMap<uint32, UHoudiniAssetParameter*> NewParameters;

	HAPI_AssetInfo AssetInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);

	HAPI_NodeInfo NodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetNodeInfo(AssetInfo.nodeId, &NodeInfo), false);

	// Retrieve parameters.
	TArray<HAPI_ParmInfo> ParmInfos;
	ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParameters(AssetInfo.nodeId, &ParmInfos[0], 0, NodeInfo.parmCount), false);

	// Retrieve integer values for this asset.
	TArray<int> ParmValueInts;
	ParmValueInts.SetNumZeroed(NodeInfo.parmIntValueCount);
	if(NodeInfo.parmIntValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmIntValues(AssetInfo.nodeId, &ParmValueInts[0], 0, NodeInfo.parmIntValueCount), false);
	}

	// Retrieve float values for this asset.
	TArray<float> ParmValueFloats;
	ParmValueFloats.SetNumZeroed(NodeInfo.parmFloatValueCount);
	if(NodeInfo.parmFloatValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmFloatValues(AssetInfo.nodeId, &ParmValueFloats[0], 0, NodeInfo.parmFloatValueCount), false);
	}

	// Retrieve string values for this asset.
	TArray<HAPI_StringHandle> ParmValueStrings;
	ParmValueStrings.SetNumZeroed(NodeInfo.parmStringValueCount);
	if(NodeInfo.parmStringValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmStringValues(AssetInfo.nodeId, true, &ParmValueStrings[0], 0, NodeInfo.parmStringValueCount), false);
	}

	// Create properties for parameters.
	for(int ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
	{
		// Retrieve param info at this index.
		const HAPI_ParmInfo& ParmInfo = ParmInfos[ParamIdx];

		// If parameter is invisible, skip it.
		if(ParmInfo.invisible)
		{
			continue;
		}

		// See if this parameter has already been created.
		uint32 ParameterHash = UHoudiniAssetParameter::GetParameterHash(AssetInfo.nodeId, ParmInfo.id);
		HoudiniAssetParameter = FindHoudiniAssetParameter(ParameterHash);

		// If parameter exists, we can reuse it.
		if(HoudiniAssetParameter)
		{
			// Remove parameter from current map.
			RemoveHoudiniAssetParameter(ParameterHash);

			// Reinitialize parameter and add it to map.
			HoudiniAssetParameter->CreateParameter(this, AssetInfo.nodeId, ParmInfo);
			NewParameters.Add(ParameterHash, HoudiniAssetParameter);
			continue;
		}

		// Skip unsupported param types for now.
		switch(ParmInfo.type)
		{
			case HAPI_PARMTYPE_STRING:
			{
				if(!ParmInfo.choiceCount)
				{
					HoudiniAssetParameter = UHoudiniAssetParameterString::Create(this, AssetInfo.nodeId, ParmInfo);
				}
				else
				{
					HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(this, AssetInfo.nodeId, ParmInfo);
				}

				break;
			}

			case HAPI_PARMTYPE_INT:
			{
				if(!ParmInfo.choiceCount)
				{
					HoudiniAssetParameter = UHoudiniAssetParameterInt::Create(this, AssetInfo.nodeId, ParmInfo);
				}
				else
				{
					HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(this, AssetInfo.nodeId, ParmInfo);
				}

				break;
			}

			case HAPI_PARMTYPE_FLOAT:
			{
				HoudiniAssetParameter = UHoudiniAssetParameterFloat::Create(this, AssetInfo.nodeId, ParmInfo);
				break;
			}

			case HAPI_PARMTYPE_TOGGLE:
			{
				HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create(this, AssetInfo.nodeId, ParmInfo);
				break;
			}

			case HAPI_PARMTYPE_COLOR:
			case HAPI_PARMTYPE_PATH_NODE:
			default:
			{
				// Just ignore unsupported types for now.
				continue;
			}
		}

		// Add this parameter to the map.
		NewParameters.Add(ParameterHash, HoudiniAssetParameter);
	}

	// Remove all unused parameters.
	ClearParameters();
	Parameters = NewParameters;

	return true;
}


void
UHoudiniAssetComponent::ClearParameters()
{
	for(TMap<uint32, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		HoudiniAssetParameter->ConditionalBeginDestroy();
	}

	Parameters.Empty();
}


void
UHoudiniAssetComponent::NotifyParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	bParametersChanged = true;
	StartHoudiniTicking();
}


void
UHoudiniAssetComponent::UploadChangedParameters()
{
	// Upload inputs.
	for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
	{
		UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;

		// If input has changed, upload it to HAPI.
		if(HoudiniAssetInput->HasChanged())
		{
			HoudiniAssetInput->UploadParameterValue();
		}
	}

	// Upload parameters.
	for(TMap<uint32, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

		// If parameter has changed, upload it to HAPI.
		if(HoudiniAssetParameter->HasChanged())
		{
			HoudiniAssetParameter->UploadParameterValue();
		}
	}

	// We no longer have changed parameters.
	bParametersChanged = false;
}


void
UHoudiniAssetComponent::CreateInputs()
{
	if(!FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		// There's no Houdini asset, we can return.
		return;
	}

	// Inputs have been created already.
	if(Inputs.Num() > 0)
	{
		return;
	}

	HAPI_AssetInfo AssetInfo;
	int32 InputCount = 0;
	if((HAPI_RESULT_SUCCESS == HAPI_GetAssetInfo(AssetId, &AssetInfo)) && AssetInfo.hasEverCooked)
	{
		InputCount = AssetInfo.geoInputCount;
	}

	// Create inputs.
	Inputs.SetNumZeroed(InputCount);
	for(int InputIdx = 0; InputIdx < InputCount; ++InputIdx)
	{
		Inputs[InputIdx] = UHoudiniAssetInput::Create(this, InputIdx);
	}
}


void
UHoudiniAssetComponent::ClearInputs()
{
	for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
	{
		UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;

		// Destroy connected Houdini asset.
		HoudiniAssetInput->DestroyHoudiniAsset();
		HoudiniAssetInput->ConditionalBeginDestroy();
	}

	Inputs.Empty();
}


void
UHoudiniAssetComponent::CreateInstanceInputs(const TArray<FHoudiniGeoPartObject>& Instancers)
{
	TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*> NewInstanceInputs;

	for(TArray<FHoudiniGeoPartObject>::TConstIterator Iter(Instancers); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = *Iter;

		// Check if this instance input already exists.
		UHoudiniAssetInstanceInput* const* FoundHoudiniAssetInstanceInput = InstanceInputs.Find(HoudiniGeoPartObject.ObjectId);
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

		if(FoundHoudiniAssetInstanceInput)
		{
			// Input already exists, we can reuse it.
			HoudiniAssetInstanceInput = *FoundHoudiniAssetInstanceInput;

			// Remove it from old map.
			InstanceInputs.Remove(HoudiniGeoPartObject.ObjectId);
		}
		else
		{
			// Otherwise we need to create new instance input.
			HoudiniAssetInstanceInput = UHoudiniAssetInstanceInput::Create(this, HoudiniGeoPartObject.ObjectId, HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId);
		}

		// Add input to new map.
		NewInstanceInputs.Add(HoudiniGeoPartObject.ObjectId, HoudiniAssetInstanceInput);

		// Create or re-create this input.
		HoudiniAssetInstanceInput->CreateInstanceInput();
	}

	ClearInstanceInputs();
	InstanceInputs = NewInstanceInputs;
}


void
UHoudiniAssetComponent::ClearInstanceInputs()
{
	for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
	{
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();
		HoudiniAssetInstanceInput->ConditionalBeginDestroy();
	}

	InstanceInputs.Empty();
}


UStaticMesh*
UHoudiniAssetComponent::LocateStaticMesh(const FHoudiniGeoPartObject& HoudiniGeoPartObject) const
{
	UStaticMesh* const* FoundStaticMesh = StaticMeshes.Find(HoudiniGeoPartObject);
	UStaticMesh* StaticMesh = nullptr;

	if(FoundStaticMesh)
	{
		StaticMesh = *FoundStaticMesh;
	}

	return StaticMesh;
}


void
UHoudiniAssetComponent::SerializeParameters(FArchive& Ar)
{
	// If we are loading, we want to clean up all existing parameters.
	if(Ar.IsLoading())
	{
		ClearParameters();
	}

	// Serialize number of parameters.
	int32 ParamCount = Parameters.Num();
	Ar << ParamCount;

	if(Ar.IsSaving())
	{
		for(TMap<uint32, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
		{
			uint32 HoudiniAssetParameterKey = IterParams.Key();
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

			Ar << HoudiniAssetParameterKey;
			Ar << HoudiniAssetParameter;
		}
	}
	else if(Ar.IsLoading())
	{
		for(int32 ParmIdx = 0; ParmIdx < ParamCount; ++ParmIdx)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;
			uint32 HoudiniAssetParameterKey = -1;

			Ar << HoudiniAssetParameterKey;
			Ar << HoudiniAssetParameter;

			HoudiniAssetParameter->SetHoudiniAssetComponent(this);
			Parameters.Add(HoudiniAssetParameterKey, HoudiniAssetParameter);
		}
	}
}


#undef HOUDINI_TEST_LOG_MESSAGE
