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
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(HoudiniAssetComponent->Parameters); IterParams; ++IterParams)
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
	CreateStaticMeshHoudiniLogoResource(StaticMeshes);

	// Release all instanced mesh resources.
	//MarkAllInstancersUnused();
	//ClearAllUnusedInstancers();

	bIsPreviewComponent = false;
	if(!InHoudiniAsset)
	{
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
	bool bFinishedLoadedInstantiation = false;

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

						if(TaskInfo.bLoadedComponent)
						{
							bFinishedLoadedInstantiation = true;
						}
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
								if(NewStaticMeshes.Num() > 0)
								{
									CreateObjectGeoPartResources(NewStaticMeshes);
								}
								else
								{
									CreateStaticMeshHoudiniLogoResource(NewStaticMeshes);
								}
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

					if(TaskInfo.bLoadedComponent)
					{
						bFinishedLoadedInstantiation = true;
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
			Task.bLoadedComponent = true;
			FHoudiniEngine::Get().AddTask(Task);
		}
		else
		{
			if(bFinishedLoadedInstantiation)
			{
				// Update parameter node id for all loaded parameters.
				UpdateLoadedParameter();

				// Additionally, we need to update and create assets for all input parameters that have geos assigned.
				UpateLoadedInputs();

				// If we finished loading instantiation, we can restore preset data.
				if(PresetBuffer.Num() > 0)
				{
					FHoudiniEngineUtils::SetAssetPreset(AssetId, PresetBuffer);
					PresetBuffer.Empty();
				}
			}

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
			CreateStaticMeshHoudiniLogoResource(StaticMeshes);

			ChangedHoudiniAsset = nullptr;
			AssetId = -1;
		}

		// Start ticking which will update the asset. We cannot update it here as it involves potential property
		// updates. It cannot be done here because this event is fired on a property which we might change.
		StartHoudiniAssetChange();
	}
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();

	// Create Houdini logo static mesh and component for it.
	CreateStaticMeshHoudiniLogoResource(StaticMeshes);
}


void
UHoudiniAssetComponent::OnComponentDestroyed()
{
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
UHoudiniAssetComponent::CreateStaticMeshHoudiniLogoResource(TMap<FHoudiniGeoPartObject, UStaticMesh*>& StaticMeshMap)
{
	if(!bIsNativeComponent)
	{
		return;
	}

	// Create Houdini logo static mesh and component for it.
	FHoudiniGeoPartObject HoudiniGeoPartObject;
	StaticMeshMap.Add(HoudiniGeoPartObject, FHoudiniEngine::Get().GetHoudiniLogoStaticMesh());
	CreateObjectGeoPartResources(StaticMeshMap);
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
	Super::PreSave();
}


void
UHoudiniAssetComponent::PostLoad()
{
	Super::PostLoad();

	// We loaded a component which has no asset associated with it.
	if(!HoudiniAsset)
	{
		// Set geometry to be Houdini logo geometry, since we have no other geometry.
		CreateStaticMeshHoudiniLogoResource(StaticMeshes);
		return;
	}

	if(StaticMeshes.Num() > 0)
	{
		CreateObjectGeoPartResources(StaticMeshes);
	}
	else
	{
		CreateStaticMeshHoudiniLogoResource(StaticMeshes);
	}

	// Perform post load initialization on instance inputs.
	PostLoadInitializeInstanceInputs();
}


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsTransacting())
	{
		// We have no support for transactions (undo system) right now.
		return;
	}

	if(!Ar.IsSaving() && !Ar.IsLoading())
	{
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

	// Serialization of preset.
	{
		bool bPresetSaved = false;
		if(Ar.IsSaving() && FHoudiniEngineUtils::IsValidAssetId(AssetId))
		{
			if(FHoudiniEngineUtils::GetAssetPreset(AssetId, PresetBuffer))
			{
				bPresetSaved = true;
			}
		}

		Ar << bPresetSaved;

		if(bPresetSaved)
		{
			Ar << PresetBuffer;

			if(Ar.IsSaving())
			{
				// We no longer need preset buffer.
				PresetBuffer.Empty();
			}
		}
	}

	// Serialize parameters.
	SerializeParameters(Ar);

	// Serialize inputs.
	SerializeInputs(Ar);

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

	// Serialize instance inputs (we do this after geometry loading as we might need it).
	SerializeInstanceInputs(Ar);

	if(Ar.IsLoading())
	{
		// We need to recompute bounding volume.
		//ComputeComponentBoundingVolume();
	}

	if(Ar.IsLoading() && bIsNativeComponent)
	{
		// This component has been loaded.
		bLoadedComponent = true;
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


FHoudiniGeoPartObject
UHoudiniAssetComponent::LocateGeoPartObject(UStaticMesh* StaticMesh) const
{
	FHoudiniGeoPartObject GeoPartObject;

	const FHoudiniGeoPartObject* FoundGeoPartObject = StaticMeshes.FindKey(StaticMesh);
	if(FoundGeoPartObject)
	{
		GeoPartObject = *FoundGeoPartObject;
	}

	return GeoPartObject;
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
	TMap<FString, UHoudiniAssetParameter*> NewParameters;

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

		FString ParameterName;
		if(!UHoudiniAssetParameter::RetrieveParameterName(ParmInfo, ParameterName))
		{
			// We had trouble retrieving name of this parameter, skip it.
			continue;
		}

		// See if this parameter has already been created.
		UHoudiniAssetParameter* const* FoundHoudiniAssetParameter = Parameters.Find(ParameterName);
		UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

		// If parameter exists, we can reuse it.
		if(FoundHoudiniAssetParameter)
		{
			HoudiniAssetParameter = *FoundHoudiniAssetParameter;

			// Remove parameter from current map.
			Parameters.Remove(ParameterName);

			// Reinitialize parameter and add it to map.
			HoudiniAssetParameter->CreateParameter(this, AssetInfo.nodeId, ParmInfo);
			NewParameters.Add(ParameterName, HoudiniAssetParameter);
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
		NewParameters.Add(ParameterName, HoudiniAssetParameter);
	}

	// Remove all unused parameters.
	ClearParameters();
	Parameters = NewParameters;

	return true;
}


void
UHoudiniAssetComponent::ClearParameters()
{
	for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		HoudiniAssetParameter->ConditionalBeginDestroy();
	}

	Parameters.Empty();
}


void
UHoudiniAssetComponent::NotifyParameterChanged(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	if(bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		bLoadedComponentRequiresInstantiation = true;
	}

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
	for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
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
UHoudiniAssetComponent::UpdateLoadedParameter()
{
	HAPI_AssetInfo AssetInfo;
	if(HAPI_RESULT_SUCCESS != HAPI_GetAssetInfo(AssetId, &AssetInfo))
	{
		return;
	}

	for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
	{
		UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();
		HoudiniAssetParameter->SetNodeId(AssetInfo.nodeId);
	}
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
UHoudiniAssetComponent::UpateLoadedInputs()
{
	for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
	{
		UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
		HoudiniAssetInput->UploadParameterValue();
	}
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
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
		{
			FString HoudiniAssetParameterKey = IterParams.Key();
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

			Ar << HoudiniAssetParameterKey;
			Ar << HoudiniAssetParameter;
		}
	}
	else if(Ar.IsLoading())
	{
		for(int32 ParmIdx = 0; ParmIdx < ParamCount; ++ParmIdx)
		{
			FString HoudiniAssetParameterKey = TEXT("");
			UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

			Ar << HoudiniAssetParameterKey;
			Ar << HoudiniAssetParameter;

			HoudiniAssetParameter->SetHoudiniAssetComponent(this);
			Parameters.Add(HoudiniAssetParameterKey, HoudiniAssetParameter);
		}
	}
}


void
UHoudiniAssetComponent::SerializeInputs(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		ClearInputs();
	}

	Ar << Inputs;

	if(Ar.IsLoading())
	{
		for(int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
		{
			Inputs[InputIdx]->SetHoudiniAssetComponent(this);
		}
	}
}


void
UHoudiniAssetComponent::SerializeInstanceInputs(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		ClearInstanceInputs();
	}

	// Serialize number of instance inputs.
	int32 InstanceInputCount = InstanceInputs.Num();
	Ar << InstanceInputCount;

	if(Ar.IsSaving())
	{
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
		{
			HAPI_ObjectId HoudiniInstanceInputKey = IterInstanceInputs.Key();
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();

			Ar << HoudiniInstanceInputKey;
			Ar << HoudiniAssetInstanceInput;
		}
	}
	else if(Ar.IsLoading())
	{
		for(int32 InstanceInputIdx = 0; InstanceInputIdx < InstanceInputCount; ++InstanceInputIdx)
		{
			HAPI_ObjectId HoudiniInstanceInputKey = -1;
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = nullptr;

			Ar << HoudiniInstanceInputKey;
			Ar << HoudiniAssetInstanceInput;

			HoudiniAssetInstanceInput->SetHoudiniAssetComponent(this);
			InstanceInputs.Add(HoudiniInstanceInputKey, HoudiniAssetInstanceInput);
		}
	}
}


void
UHoudiniAssetComponent::PostLoadInitializeInstanceInputs()
{
	for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator IterInstanceInputs(InstanceInputs); IterInstanceInputs; ++IterInstanceInputs)
	{
		UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = IterInstanceInputs.Value();
		HoudiniAssetInstanceInput->CreateInstanceInputPostLoad();
	}
}


#undef HOUDINI_TEST_LOG_MESSAGE
