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


// Macro to update given property on all components.
#define HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(COMPONENT_CLASS, PROPERTY)										\
	do																										\
	{																										\
		TArray<UActorComponent*> ReregisterComponents;														\
		for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)							\
		{																									\
			COMPONENT_CLASS* Component = Cast<COMPONENT_CLASS>(*Iter);										\
			if(Component)																					\
			{																								\
				Component->PROPERTY = PROPERTY;																\
				ReregisterComponents.Add(Component);														\
			}																								\
		}																									\
																											\
		if(ReregisterComponents.Num() > 0)																	\
		{																									\
			FMultiComponentReregisterContext MultiComponentReregisterContext(ReregisterComponents);			\
		}																									\
	}																										\
	while(0)


bool
UHoudiniAssetComponent::bDisplayEngineNotInitialized = true;


bool
UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch = true;


UHoudiniAssetComponent::UHoudiniAssetComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	HoudiniAsset(nullptr),
	AssetId(-1),
	HapiNotificationStarted(0.0),
	bEnableCooking(true),
	bUploadTransformsToHoudiniEngine(true),
	bTransformChangeTriggersCooks(false),
	bContainsHoudiniLogoGeometry(false),
	bIsNativeComponent(false),
	bIsPreviewComponent(false),
	bLoadedComponent(false),
	bLoadedComponentRequiresInstantiation(false),
	bInstantiated(false),
	bIsPlayModeActive(false),
	bParametersChanged(false),
	bCurveChanged(false),
	bComponentTransformHasChanged(false),
	bUndoRequested(false),
	bManualRecook(false)
{
	UObject* Object = ObjectInitializer.GetObj();
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

	// Initialize static mesh generation parameters.
	bGeneratedDoubleSidedGeometry = false;
	GeneratedPhysMaterial = nullptr;
	GeneratedCollisionTraceFlag = CTF_UseDefault;
	GeneratedLpvBiasMultiplier = 1.0f;
	GeneratedLightMapResolution = 32;
	GeneratedLightMapCoordinateIndex = 1;
	bGeneratedUseMaximumStreamingTexelRatio = false;
	GeneratedStreamingDistanceMultiplier = 1.0f;

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

		// Add references to all spline components.
		for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(HoudiniAssetComponent->SplineComponents); Iter; ++Iter)
		{
			UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();
			Collector.AddReferencedObject(HoudiniSplineComponent, InThis);
		}

		// Retrieve asset associated with this component and add reference to it.
		UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
		if(HoudiniAsset)
		{
			// Manually add a reference to Houdini asset from this component.
			Collector.AddReferencedObject(HoudiniAsset, InThis);
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

	UHoudiniAsset* Asset = nullptr;
	AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(GetOwner());

	HoudiniAsset = InHoudiniAsset;

	if(!bIsNativeComponent)
	{
		return;
	}

	// Houdini Asset has been changed, we need to reset corresponding HDA and relevant resources.
	ResetHoudiniResources();

	// Clear all created parameters.
	ClearParameters();

	// Clear all inputs.
	ClearInputs();

	// Clear all instance inputs.
	ClearInstanceInputs();

	// Release all curve related resources.
	ClearCurves();

	// Set Houdini logo to be default geometry.
	ReleaseObjectGeoPartResources(StaticMeshes);
	StaticMeshes.Empty();
	StaticMeshComponents.Empty();
	CreateStaticMeshHoudiniLogoResource(StaticMeshes);

	bIsPreviewComponent = false;
	if(!InHoudiniAsset)
	{
		UpdateEditorProperties();
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

	// Register component visualizers with this module. This is a good place to perform this task as at this point
	// all modules, including Editor modules will have been loaded. We cannot perform this task inside Houdini Engine
	// module as there's no guarantee that necessary modules were initialized.
	HoudiniEngine.RegisterComponentVisualizers();

	if(!bIsPreviewComponent)
	{
		if(HoudiniEngine.IsInitialized())
		{
			if(!bLoadedComponent)
			{
				StartTaskAssetInstantiation(false, true);
			}
		}
		else
		{
			if(UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch && HoudiniEngine.CheckHapiVersionMismatch())
			{
				// We have mismatch in defined and running versions.
				int32 RunningEngineMajor = 0;
				int32 RunningEngineMinor = 0;
				int32 RunningEngineApi = 0;

				// Retrieve version numbers for running Houdini Engine.
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MAJOR, &RunningEngineMajor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_MINOR, &RunningEngineMinor);
				FHoudiniApi::GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_ENGINE_API, &RunningEngineApi);

				FString WarningMessage = FString::Printf(TEXT("Defined version: %d.%d.api:%d vs Running version: %d.%d.api:%d mismatch. ")
														 TEXT("libHAPI.dll was loaded, but has wrong version. ")
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

				UHoudiniAssetComponent::bDisplayEngineHapiVersionMismatch = false;
			}
			else if(UHoudiniAssetComponent::bDisplayEngineNotInitialized)
			{
				// If this is first time component is instantiated and we do not have Houdini Engine initialized, display message.
				FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
				FText WarningTitleText = FText::FromString(WarningTitle);
				FString WarningMessage = TEXT("Houdini Installation was not detected. ")
										 TEXT("Failed to locate or load libHAPI.dll. ")
										 TEXT("No cooking / instantiation will take place.");
				FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);

				UHoudiniAssetComponent::bDisplayEngineNotInitialized = false;
			}
		}
	}
}


bool
UHoudiniAssetComponent::IsNotCookingOrInstantiating() const
{
	return !HapiGUID.IsValid();
}


void
UHoudiniAssetComponent::AssignUniqueActorLabel()
{
	if(GEditor && FHoudiniEngineUtils::IsValidAssetId(AssetId))
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

				StaticMeshComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);

				//StaticMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
				//StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
				//StaticMeshComponent->bAlwaysCreatePhysicsState = true;

				StaticMeshComponent->SetStaticMesh(StaticMesh);
				StaticMeshComponent->SetVisibility(true);
				StaticMeshComponent->RegisterComponent();
			}

			// Transform the component by transformation provided by HAPI.
			StaticMeshComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);
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

		// Create necessary curves.
		CreateCurves(FoundCurves);
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
	if(!TimerDelegateCooking.IsBound() && GEditor)
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
	if(TimerDelegateCooking.IsBound() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(TimerDelegateCooking);
		TimerDelegateCooking.Unbind();

		// Reset time for delayed notification.
		HapiNotificationStarted = 0.0;
	}
}


void
UHoudiniAssetComponent::TickHoudiniComponent()
{
	// Get settings.
	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	FHoudiniEngineTaskInfo TaskInfo;
	bool bStopTicking = false;
	bool bFinishedLoadedInstantiation = false;

	static float NotificationFadeOutDuration = 2.0f;
	static float NotificationExpireDuration = 2.0f;
	static double NotificationUpdateFrequency = 2.0f;

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
							FTransform ComponentTransform;
							TMap<FHoudiniGeoPartObject, UStaticMesh*> NewStaticMeshes;
							if(FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(this, GetOutermost(), StaticMeshes, NewStaticMeshes, ComponentTransform))
							{
								// Remove all duplicates. After this operation, old map will have meshes which we need to deallocate.
								for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(NewStaticMeshes); Iter; ++Iter)
								{
									const FHoudiniGeoPartObject HoudiniGeoPartObject = Iter.Key();
									UStaticMesh* StaticMesh = Iter.Value();

									// Remove mesh from previous map of meshes.
									UStaticMesh* const* FoundOldStaticMesh = StaticMeshes.Find(HoudiniGeoPartObject);
									if(FoundOldStaticMesh)
									{
										UStaticMesh* OldStaticMesh = *FoundOldStaticMesh;

										if(OldStaticMesh == StaticMesh)
										{
											// Mesh has not changed, we need to remove it from old map to avoid deallocation.
											StaticMeshes.Remove(HoudiniGeoPartObject);
										}
									}
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

	if(!HapiGUID.IsValid() && (bInstantiated || bParametersChanged || bCurveChanged || bComponentTransformHasChanged))
	{
		// If we are not cooking and we have property changes queued up.

		// Grab current time for delayed notification.
		HapiNotificationStarted = FPlatformTime::Seconds();

		// This component has been loaded and requires instantiation.
		if(bLoadedComponentRequiresInstantiation)
		{
			bLoadedComponentRequiresInstantiation = false;

			StartTaskAssetInstantiation(true);
		}
		else
		{
			// If we are doing first cook after instantiation after loading or if cooking is enabled and undo is invoked.
			if(bFinishedLoadedInstantiation || (bEnableCooking && bUndoRequested))
			{
				// Update parameter node id for all loaded parameters.
				UpdateLoadedParameter();

				// Additionally, we need to update and create assets for all input parameters that have geos assigned.
				UpdateLoadedInputs();

				// We also need to upload loaded curve points.
				UploadLoadedCurves();

				// If we finished loading instantiation, we can restore preset data.
				if(PresetBuffer.Num() > 0)
				{
					FHoudiniEngineUtils::SetAssetPreset(AssetId, PresetBuffer);
					PresetBuffer.Empty();
				}

				// Unset Undo flag.
				bUndoRequested = false;
			}

			if(bFinishedLoadedInstantiation)
			{
				// Upload changed parameters back to HAPI.
				UploadChangedParameters();

				// Create asset cooking task object and submit it for processing.
				StartTaskAssetCooking();
			}
			else
			{
				// If we have changed transformation, we need to upload it. Also record flag of whether we need to recook.
				bool bTransformRecook = false;
				if(bComponentTransformHasChanged)
				{
					UploadChangedTransform();

					if(bTransformChangeTriggersCooks)
					{
						bTransformRecook = true;
					}
				}

				// Compute whether we need to cook.
				if(bInstantiated || bParametersChanged || bTransformRecook || bCurveChanged)
				{
					if(bEnableCooking || bManualRecook || bInstantiated)
					{
						// Upload changed parameters back to HAPI.
						UploadChangedParameters();

						// Create asset cooking task object and submit it for processing.
						StartTaskAssetCooking();

						// Reset curves flag.
						bCurveChanged = false;
					}
				}

				// Reset manual recook flag.
				bManualRecook = false;
			}
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
UHoudiniAssetComponent::UpdateEditorProperties()
{
	AHoudiniAssetActor* HoudiniAssetActor = GetHoudiniAssetActorOwner();
	if(GEditor && HoudiniAssetActor && bIsNativeComponent)
	{
		// Manually reselect the actor - this will cause details panel to be updated and force our
		// property changes to be picked up by the UI.
		GEditor->SelectActor(HoudiniAssetActor, true, true);

		// Notify the editor about selection change.
		GEditor->NoteSelectionChange();
	}
}


void
UHoudiniAssetComponent::GetAllUsedStaticMeshes(TArray<UStaticMesh*>& UsedStaticMeshes)
{
	UsedStaticMeshes.Empty();

	// Add all static meshes.
	for(TMap<FHoudiniGeoPartObject, UStaticMesh*>::TIterator Iter(StaticMeshes); Iter; ++Iter)
	{
		UStaticMesh* StaticMesh = Iter.Value();
		if(StaticMesh)
		{
			UsedStaticMeshes.Add(StaticMesh);
		}
	}
}


void
UHoudiniAssetComponent::StartTaskAssetInstantiation(bool bLoadedComponent, bool bStartTicking)
{
	// Create new GUID to identify this request.
	HapiGUID = FGuid::NewGuid();

	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetInstantiation, HapiGUID);
	Task.Asset = HoudiniAsset;
	Task.ActorName = GetOuter()->GetName();
	Task.bLoadedComponent = bLoadedComponent;
	FHoudiniEngine::Get().AddTask(Task);

	// Start ticking - this will poll the cooking system for completion.
	if(bStartTicking)
	{
		StartHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::StartTaskAssetDeletion()
{
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

		// We do not need to tick as we are not interested in result - this component is about to be deleted.
	}
}


void
UHoudiniAssetComponent::StartTaskAssetCooking(bool bStartTicking)
{
	// Generate GUID for our new task.
	HapiGUID = FGuid::NewGuid();

	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, HapiGUID);
	Task.ActorName = GetOuter()->GetName();
	Task.AssetComponent = this;
	FHoudiniEngine::Get().AddTask(Task);

	if(bStartTicking)
	{
		StartHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::StartTaskAssetCookingManual()
{
	bManualRecook = true;
	StartTaskAssetCooking(true);
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
UHoudiniAssetComponent::OnUpdateTransform(bool bSkipPhysicsMove)
{
	Super::OnUpdateTransform(bSkipPhysicsMove);

	// If we have a valid asset id and asset has been cooked.
	if(FHoudiniEngineUtils::IsValidAssetId(AssetId) && !bInstantiated)
	{
		// If transform update push to HAPI is enabled.
		if(bUploadTransformsToHoudiniEngine)
		{
			// Flag asset for recooking.
			bComponentTransformHasChanged = true;
			StartHoudiniTicking();
		}
	}
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

	// Start asset deletion.
	StartTaskAssetDeletion();
}


void
UHoudiniAssetComponent::UpdateRenderingInformation()
{
	// Need to send this to render thread at some point.
	MarkRenderStateDirty();

	// Update physics representation right away.
	RecreatePhysicsState();
	for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)
	{
		USceneComponent* SceneComponent = *Iter;
		SceneComponent->RecreatePhysicsState();
	}

	// Since we have new asset, we need to update bounds.
	UpdateBounds();
}


void
UHoudiniAssetComponent::SubscribeEditorDelegates()
{
	// Add begin and end delegates for play-in-editor.
	FEditorDelegates::BeginPIE.AddUObject(this, &UHoudiniAssetComponent::OnPIEEventBegin);
	FEditorDelegates::EndPIE.AddUObject(this, &UHoudiniAssetComponent::OnPIEEventEnd);
}


void
UHoudiniAssetComponent::UnsubscribeEditorDelegates()
{
	// Remove begin and end delegates for play-in-editor.
	FEditorDelegates::BeginPIE.RemoveUObject(this, &UHoudiniAssetComponent::OnPIEEventBegin);
	FEditorDelegates::EndPIE.RemoveUObject(this, &UHoudiniAssetComponent::OnPIEEventEnd);
}


void
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!bIsNativeComponent)
	{
		return;
	}

	UProperty* Property = PropertyChangedEvent.MemberProperty;

	if(!Property)
	{
		return;
	}

	if(Property->GetName() == TEXT("Mobility"))
	{
		// Mobility was changed, we need to update it for all attached components as well.
		for(TArray<USceneComponent*>::TIterator Iter(AttachChildren); Iter; ++Iter)
		{
			USceneComponent* SceneComponent = *Iter;
			SceneComponent->SetMobility(Mobility);
		}

		return;
	}
	else if(Property->GetName() == TEXT("bVisible"))
	{
		// Visibility has changed, propagate it to children.
		SetVisibility(bVisible, true);
		return;
	}

	if(Property->HasMetaData(TEXT("Category")))
	{
		const FString& Category = Property->GetMetaData(TEXT("Category"));
		static const FString CategoryHoudiniGeneratedStaticMeshSettings = TEXT("HoudiniGeneratedStaticMeshSettings");
		static const FString CategoryLighting = TEXT("Lighting");

		if(CategoryHoudiniGeneratedStaticMeshSettings == Category)
		{
			// We are changing one of the mesh generation properties, we need to update all static meshes.
			for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(StaticMeshComponents); Iter; ++Iter)
			{
				UStaticMesh* StaticMesh = Iter.Key();

				SetStaticMeshGenerationParameters(StaticMesh);
				FHoudiniScopedGlobalSilence ScopedGlobalSilence;
				StaticMesh->Build(true);
				RefreshCollisionChange(StaticMesh);
			}

			return;
		}
		else if(CategoryLighting == Category)
		{
			if(Property->GetName() == TEXT("CastShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, CastShadow);
			}
			else if(Property->GetName() == TEXT("bCastDynamicShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastDynamicShadow);
			}
			else if(Property->GetName() == TEXT("bCastStaticShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastStaticShadow);
			}
			else if(Property->GetName() == TEXT("bCastVolumetricTranslucentShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastVolumetricTranslucentShadow);
			}
			else if(Property->GetName() == TEXT("bCastInsetShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastInsetShadow);
			}
			else if(Property->GetName() == TEXT("bCastHiddenShadow"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastHiddenShadow);
			}
			else if(Property->GetName() == TEXT("bCastShadowAsTwoSided"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bCastShadowAsTwoSided);
			}
			else if(Property->GetName() == TEXT("bLightAsIfStatic"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bLightAsIfStatic);
			}
			else if(Property->GetName() == TEXT("bLightAttachmentsAsGroup"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, bLightAttachmentsAsGroup);
			}
			else if(Property->GetName() == TEXT("IndirectLightingCacheQuality"))
			{
				HOUDINI_UPDATE_ALL_CHILD_COMPONENTS(UPrimitiveComponent, IndirectLightingCacheQuality);
			}
		}
	}
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();

	// Create Houdini logo static mesh and component for it.
	CreateStaticMeshHoudiniLogoResource(StaticMeshes);

	// Subscribe to delegates.
	SubscribeEditorDelegates();
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
	ClearCurves();

	// Destroy all parameters.
	ClearParameters();

	// Destroy all inputs.
	ClearInputs();

	// Destroy all instance inputs.
	ClearInstanceInputs();

	// Unsubscribe from Editor events.
	UnsubscribeEditorDelegates();

	Super::OnComponentDestroyed();
}


void
UHoudiniAssetComponent::OnRegister()
{
	Super::OnRegister();

	// We need to recreate render states for loaded components.
	if(bLoadedComponent)
	{
		// Static meshes.
		for(TMap<UStaticMesh*, UStaticMeshComponent*>::TIterator Iter(StaticMeshComponents); Iter; ++Iter)
		{
			UStaticMeshComponent* StaticMeshComponent = Iter.Value();
			if(StaticMeshComponent)
			{
				// Recreate render state.
				StaticMeshComponent->RecreateRenderState_Concurrent();

				// Need to recreate physics state.
				StaticMeshComponent->RecreatePhysicsState();
			}
		}

		// Instanced static meshes.
		for(TMap<HAPI_ObjectId, UHoudiniAssetInstanceInput*>::TIterator Iter(InstanceInputs); Iter; ++Iter)
		{
			UHoudiniAssetInstanceInput* HoudiniAssetInstanceInput = Iter.Value();

			// Recreate render state.
			HoudiniAssetInstanceInput->RecreateRenderStates();

			// Recreate physics state.
			//HoudiniAssetInstanceInput->RecreatePhysicsStates();
		}
	}
}


void
UHoudiniAssetComponent::OnUnregister()
{
	Super::OnUnregister();
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

	// Perform post load initialization of curve / spline components.
	PostLoadCurves();

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


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!Ar.IsSaving() && !Ar.IsLoading())
	{
		return;
	}

	// State of this component.
	EHoudiniAssetComponentState::Enum ComponentState = EHoudiniAssetComponentState::Invalid;

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
	SerializeEnumeration<EHoudiniAssetComponentState::Enum>(Ar, ComponentState);

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

	// If we are loading, we need to restore the asset.
	if(Ar.IsLoading())
	{
		// We need to locate corresponding package and load it if it is not loaded.
		UPackage* Package = FindPackage(nullptr, *HoudiniAssetPackage);
		if(!Package)
		{
			// Package was not loaded previously, we will try to load it.
			Package = LoadPackage(nullptr, *HoudiniAssetPackage, LOAD_None);
		}

		if(Package)
		{
			// At this point we can locate the asset, since package exists.
			UHoudiniAsset* HoudiniAssetLookup = Cast<UHoudiniAsset>(StaticFindObject(UHoudiniAsset::StaticClass(), Package, *HoudiniAssetName, true));
			if(HoudiniAssetLookup)
			{
				HoudiniAsset = HoudiniAssetLookup;
			}
		}
	}

	// If we have no asset, we can stop.
	if(!HoudiniAsset)
	{
		return;
	}

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
					FHoudiniEngineUtils::SaveRawStaticMesh(StaticMesh, nullptr, Ar);
				}
			}
			else if(Ar.IsLoading())
			{
				for(int32 StaticMeshIdx = 0; StaticMeshIdx < NumStaticMeshes; ++StaticMeshIdx)
				{
					FHoudiniGeoPartObject HoudiniGeoPartObject;

					// Get object geo part information.
					HoudiniGeoPartObject.Serialize(Ar);

					// Load the raw mesh.
					UStaticMesh* LoadedStaticMesh = nullptr;
					if(!HoudiniGeoPartObject.IsInstancer() && !HoudiniGeoPartObject.IsCurve())
					{
						LoadedStaticMesh = FHoudiniEngineUtils::LoadRawStaticMesh(this, HoudiniGeoPartObject, nullptr, Ar);

						// See if we already have a static mesh for this geo part object.
						UStaticMesh* const* FoundOldStaticMesh = StaticMeshes.Find(HoudiniGeoPartObject);
						if(FoundOldStaticMesh)
						{
							UStaticMesh* OldStaticMesh = *FoundOldStaticMesh;

							// Retrieve component for old static mesh.
							UStaticMeshComponent* const* FoundOldStaticMeshComponent = StaticMeshComponents.Find(OldStaticMesh);
							if(FoundOldStaticMeshComponent)
							{
								UStaticMeshComponent* OldStaticMeshComponent = *FoundOldStaticMeshComponent;

								// We need to replace component's static mesh with a new one.
								StaticMeshComponents.Remove(OldStaticMesh);
								StaticMeshComponents.Add(LoadedStaticMesh, OldStaticMeshComponent);
								OldStaticMeshComponent->SetStaticMesh(LoadedStaticMesh);
							}
						}
					}

					StaticMeshes.Add(HoudiniGeoPartObject, LoadedStaticMesh);
				}
			}
		}
	}

	// Serialize instance inputs (we do this after geometry loading as we might need it).
	SerializeInstanceInputs(Ar);

	// Serialize curves.
	SerializeCurves(Ar);

	if(Ar.IsLoading() && bIsNativeComponent)
	{
		// This component has been loaded.
		bLoadedComponent = true;
	}

	// Serialize whether we are in PIE mode.
	Ar << bIsPlayModeActive;

	// Serialize whether we want to enable cooking for this component / asset.
	Ar << bEnableCooking;

	// Serialize whether we need to push transformations to HAPI.
	Ar << bUploadTransformsToHoudiniEngine;

	// Serialize whether we need to cook upon transformation change.
	Ar << bTransformChangeTriggersCooks;
}


void
UHoudiniAssetComponent::PreEditUndo()
{
	Super::PreEditUndo();
}


void
UHoudiniAssetComponent::PostEditUndo()
{
	Super::PostEditUndo();

	if(bLoadedComponent)
	{
		bUndoRequested = true;
		bParametersChanged = true;

		StartHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::PostInitProperties()
{
	Super::PostInitProperties();

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	// Copy defaults from settings.
	bEnableCooking = HoudiniRuntimeSettings->bEnableCooking;
	bUploadTransformsToHoudiniEngine = HoudiniRuntimeSettings->bUploadTransformsToHoudiniEngine;
	bTransformChangeTriggersCooks = HoudiniRuntimeSettings->bTransformChangeTriggersCooks;
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
UHoudiniAssetComponent::LocateStaticMeshes(int32 ObjectToInstanceId, TArray<FHoudiniGeoPartObject>& InOutObjectsToInstance) const
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


void
UHoudiniAssetComponent::CreateCurves(const TArray<FHoudiniGeoPartObject>& FoundCurves)
{
	TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*> NewSplineComponents;
	for(TArray<FHoudiniGeoPartObject>::TConstIterator Iter(FoundCurves); Iter; ++Iter)
	{
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = *Iter;

		// Retrieve node id from geo part.
		HAPI_NodeId NodeId = HoudiniGeoPartObject.GetNodeId(AssetId);
		if(-1 == NodeId)
		{
			// Invalid node id.
			continue;
		}

		if(!HoudiniGeoPartObject.HasParameters(AssetId))
		{
			// We have no parameters on this curve.
			continue;
		}

		FString CurvePointsString;
		EHoudiniSplineComponentType::Enum CurveTypeValue = EHoudiniSplineComponentType::Bezier;
		EHoudiniSplineComponentMethod::Enum CurveMethodValue = EHoudiniSplineComponentMethod::CVs;
		int32 CurveClosed = 1;

		HAPI_AttributeInfo AttributeRefinedCurvePositions;
		TArray<float> RefinedCurvePositions;
		if(!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(HoudiniGeoPartObject, HAPI_UNREAL_ATTRIB_POSITION, AttributeRefinedCurvePositions, RefinedCurvePositions))
		{
			continue;
		}

		if(!AttributeRefinedCurvePositions.exists && RefinedCurvePositions.Num() > 0)
		{
			continue;
		}

		// Transfer refined positions to position vector and perform necessary axis swap.
		TArray<FVector> CurveDisplayPoints;
		FHoudiniEngineUtils::ConvertScaleAndFlipVectorData(RefinedCurvePositions, CurveDisplayPoints);

		if(!FHoudiniEngineUtils::HapiGetParameterDataAsString(NodeId, HAPI_UNREAL_PARAM_CURVE_COORDS, TEXT(""), CurvePointsString) ||
		   !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_TYPE, (int32) EHoudiniSplineComponentType::Bezier, (int32&) CurveTypeValue) ||
		   !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_METHOD, (int32) EHoudiniSplineComponentMethod::CVs, (int32&) CurveMethodValue) ||
		   !FHoudiniEngineUtils::HapiGetParameterDataAsInteger(NodeId, HAPI_UNREAL_PARAM_CURVE_CLOSED, 1, CurveClosed))
		{
			continue;
		}

		// Process coords string and extract positions.
		TArray<FVector> CurvePoints;
		FHoudiniEngineUtils::ExtractStringPositions(CurvePointsString, CurvePoints);

		// Check if this curve already exists.
		UHoudiniSplineComponent* const* FoundHoudiniSplineComponent = SplineComponents.Find(HoudiniGeoPartObject);
		UHoudiniSplineComponent* HoudiniSplineComponent = nullptr;

		if(FoundHoudiniSplineComponent)
		{
			// Curve already exists, we can reuse it.
			HoudiniSplineComponent = *FoundHoudiniSplineComponent;

			// Remove it from old map.
			SplineComponents.Remove(HoudiniGeoPartObject);
		}
		else
		{
			// We need to create new curve.
			HoudiniSplineComponent = ConstructObject<UHoudiniSplineComponent>(UHoudiniSplineComponent::StaticClass(), this, NAME_None, RF_Transient);
			HoudiniSplineComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
			HoudiniSplineComponent->SetVisibility(true);
			HoudiniSplineComponent->RegisterComponent();
		}

		// Add to map of components.
		NewSplineComponents.Add(HoudiniGeoPartObject, HoudiniSplineComponent);

		// Transform the component by transformation provided by HAPI.
		HoudiniSplineComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

		// Construct curve from available data.
		HoudiniSplineComponent->Construct(HoudiniGeoPartObject, CurvePoints, CurveDisplayPoints, CurveTypeValue, CurveMethodValue, (CurveClosed == 1));
	}

	ClearCurves();
	SplineComponents = NewSplineComponents;
}


void
UHoudiniAssetComponent::ClearCurves()
{
	for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		UHoudiniSplineComponent* SplineComponent = Iter.Value();

		SplineComponent->DetachFromParent();
		SplineComponent->UnregisterComponent();
		SplineComponent->DestroyComponent();
	}

	SplineComponents.Empty();
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
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo), false);

	HAPI_NodeInfo NodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(AssetInfo.nodeId, &NodeInfo), false);

	if(NodeInfo.parmCount > 0)
	{
		// Retrieve parameters.
		TArray<HAPI_ParmInfo> ParmInfos;
		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetParameters(AssetInfo.nodeId, &ParmInfos[0], 0, NodeInfo.parmCount), false);

		// Create properties for parameters.
		for(int32 ParamIdx = 0; ParamIdx < NodeInfo.parmCount; ++ParamIdx)
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
				HoudiniAssetParameter->CreateParameter(this, nullptr, AssetInfo.nodeId, ParmInfo);
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
						HoudiniAssetParameter = UHoudiniAssetParameterString::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					}
					else
					{
						HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					}

					break;
				}

				case HAPI_PARMTYPE_INT:
				{
					if(!ParmInfo.choiceCount)
					{
						HoudiniAssetParameter = UHoudiniAssetParameterInt::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					}
					else
					{
						HoudiniAssetParameter = UHoudiniAssetParameterChoice::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					}

					break;
				}

				case HAPI_PARMTYPE_FLOAT:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterFloat::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_TOGGLE:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterToggle::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_COLOR:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterColor::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_LABEL:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterLabel::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_BUTTON:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterButton::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_SEPARATOR:
				{
					HoudiniAssetParameter = UHoudiniAssetParameterSeparator::Create(this, nullptr, AssetInfo.nodeId, ParmInfo);
					break;
				}

				case HAPI_PARMTYPE_PATH_FILE:
				case HAPI_PARMTYPE_PATH_FILE_GEO:
				case HAPI_PARMTYPE_PATH_FILE_IMAGE:
				{
					continue;
				}

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
UHoudiniAssetComponent::NotifyParameterWillChange(UHoudiniAssetParameter* HoudiniAssetParameter)
{
	FScopedTransaction Transaction(LOCTEXT("HoudiniParameterChange", "Houdini Parameter Change"));
	Modify();
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
UHoudiniAssetComponent::NotifyHoudiniSplineChanged(UHoudiniSplineComponent* HoudiniSplineComponent)
{
	if(bLoadedComponent && !FHoudiniEngineUtils::IsValidAssetId(AssetId))
	{
		bLoadedComponentRequiresInstantiation = true;
	}

	bCurveChanged = true;
	StartHoudiniTicking();
}


void
UHoudiniAssetComponent::SetStaticMeshGenerationParameters(UStaticMesh* StaticMesh)
{
	if(StaticMesh)
	{
		// Make sure static mesh has a new lighting guid.
		StaticMesh->LightingGuid = FGuid::NewGuid();
		StaticMesh->LODGroup = NAME_None;

		// Set resolution of lightmap.
		StaticMesh->LightMapResolution = GeneratedLightMapResolution;

		// Set Bias multiplier for Light Propagation Volume lighting.
		StaticMesh->LpvBiasMultiplier = GeneratedLpvBiasMultiplier;

		// Set light map coordinate index.
		StaticMesh->LightMapCoordinateIndex = GeneratedLightMapCoordinateIndex;

		// Set method for LOD texture factor computation.
		StaticMesh->bUseMaximumStreamingTexelRatio = bGeneratedUseMaximumStreamingTexelRatio;

		// Set distance where textures using UV 0 are streamed in/out.
		StaticMesh->StreamingDistanceMultiplier = GeneratedStreamingDistanceMultiplier;

		// Set default settings when using this mesh for instanced foliage.
		StaticMesh->FoliageDefaultSettings = GeneratedFoliageDefaultSettings;

		// Add user data.
		for(int32 AssetUserDataIdx = 0; AssetUserDataIdx < GeneratedAssetUserData.Num(); ++AssetUserDataIdx)
		{
			StaticMesh->AddAssetUserData(GeneratedAssetUserData[AssetUserDataIdx]);
		}

		StaticMesh->CreateBodySetup();
		UBodySetup* BodySetup = StaticMesh->BodySetup;
		check(BodySetup);

		// Set flag whether physics triangle mesh will use double sided faces when doing scene queries.
		BodySetup->bDoubleSidedGeometry = bGeneratedDoubleSidedGeometry;

		// Assign physical material for simple collision.
		BodySetup->PhysMaterial = GeneratedPhysMaterial;

		// Assign collision trace behavior.
		BodySetup->CollisionTraceFlag = GeneratedCollisionTraceFlag;

		// Assign walkable slope behavior.
		BodySetup->WalkableSlopeOverride = GeneratedWalkableSlopeOverride;

		// We want to use all of geometry for collision detection purposes.
		BodySetup->bMeshCollideAll = true;
	}
}


void
UHoudiniAssetComponent::UploadChangedParameters()
{
	if(bParametersChanged)
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
	}

	// We no longer have changed parameters.
	bParametersChanged = false;
}


void
UHoudiniAssetComponent::UploadChangedTransform()
{
	// Retrieve the current component-to-world transform for this component.
	const FTransform& ComponentWorldTransform = GetComponentTransform();

	// Translate Unreal transform to HAPI Euler one.
	HAPI_TransformEuler TransformEuler;
	FHoudiniEngineUtils::TranslateUnrealTransform(ComponentWorldTransform, TransformEuler);

	if(HAPI_RESULT_SUCCESS != FHoudiniApi::SetAssetTransform(GetAssetId(), &TransformEuler))
	{
		HOUDINI_LOG_MESSAGE(TEXT("Failed Uploading Transformation change back to HAPI."));
	}

	// We no longer have changed transform.
	bComponentTransformHasChanged = false;
}


void
UHoudiniAssetComponent::UpdateLoadedParameter()
{
	HAPI_AssetInfo AssetInfo;
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo))
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
	if((HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(AssetId, &AssetInfo)) && AssetInfo.hasEverCooked)
	{
		InputCount = AssetInfo.geoInputCount;
	}

	// Create inputs.
	Inputs.SetNumZeroed(InputCount);
	for(int32 InputIdx = 0; InputIdx < InputCount; ++InputIdx)
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
		HoudiniAssetInput->DestroyHoudiniAssets();
		HoudiniAssetInput->ConditionalBeginDestroy();
	}

	Inputs.Empty();
}


void
UHoudiniAssetComponent::UpdateLoadedInputs()
{
	for(TArray<UHoudiniAssetInput*>::TIterator IterInputs(Inputs); IterInputs; ++IterInputs)
	{
		UHoudiniAssetInput* HoudiniAssetInput = *IterInputs;
		HoudiniAssetInput->UploadParameterValue();
	}
}


void
UHoudiniAssetComponent::UploadLoadedCurves()
{
	for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();
		HoudiniSplineComponent->UploadControlPoints();
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
		// Load and create parameters.
		for(int32 ParmIdx = 0; ParmIdx < ParamCount; ++ParmIdx)
		{
			FString HoudiniAssetParameterKey = TEXT("");
			UHoudiniAssetParameter* HoudiniAssetParameter = nullptr;

			Ar << HoudiniAssetParameterKey;
			Ar << HoudiniAssetParameter;

			HoudiniAssetParameter->SetHoudiniAssetComponent(this);
			Parameters.Add(HoudiniAssetParameterKey, HoudiniAssetParameter);
		}

		// We need to re-patch parent parameter links.
		for(TMap<FString, UHoudiniAssetParameter*>::TIterator IterParams(Parameters); IterParams; ++IterParams)
		{
			UHoudiniAssetParameter* HoudiniAssetParameter = IterParams.Value();

			const FString& ParentParameterName = HoudiniAssetParameter->GetParentParameterName();
			if(!ParentParameterName.IsEmpty())
			{
				UHoudiniAssetParameter* const* FoundParentParameter = Parameters.Find(ParentParameterName);
				if(FoundParentParameter)
				{
					HoudiniAssetParameter->SetParentParameter(*FoundParentParameter);
				}
			}
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
UHoudiniAssetComponent::SerializeCurves(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		ClearCurves();
	}

	// Serialize number of curves.
	int32 CurveCount = SplineComponents.Num();
	Ar << CurveCount;

	if(Ar.IsSaving())
	{
		for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
		{
			FHoudiniGeoPartObject& HoudiniGeoPartObject = Iter.Key();
			UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();

			// Store the object geo part information for this curve.
			HoudiniGeoPartObject.Serialize(Ar);

			// Store component information.
			HoudiniSplineComponent->SerializeRaw(Ar);
		}
	}
	else if(Ar.IsLoading())
	{
		for(int32 CurveIdx = 0; CurveIdx < CurveCount; ++CurveIdx)
		{
			FHoudiniGeoPartObject HoudiniGeoPartObject;
			UHoudiniSplineComponent* HoudiniSplineComponent = nullptr;

			// Retrieve geo part information for this curve.
			HoudiniGeoPartObject.Serialize(Ar);

			// Create Spline component.
			HoudiniSplineComponent = ConstructObject<UHoudiniSplineComponent>(UHoudiniSplineComponent::StaticClass(), GetOwner(), NAME_None, RF_Transient);
			HoudiniSplineComponent->AddToRoot();
			HoudiniSplineComponent->SerializeRaw(Ar);

			// We need to store geo part to spline component mapping.
			SplineComponents.Add(HoudiniGeoPartObject, HoudiniSplineComponent);
		}
	}
}


void
UHoudiniAssetComponent::PostLoadCurves()
{
	for(TMap<FHoudiniGeoPartObject, UHoudiniSplineComponent*>::TIterator Iter(SplineComponents); Iter; ++Iter)
	{
		UHoudiniSplineComponent* HoudiniSplineComponent = Iter.Value();

		HoudiniSplineComponent->AttachTo(this, NAME_None, EAttachLocation::KeepRelativeOffset);
		HoudiniSplineComponent->SetVisibility(true);
		HoudiniSplineComponent->RegisterComponent();
		HoudiniSplineComponent->RemoveFromRoot();
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


void
UHoudiniAssetComponent::RemoveStaticMeshComponent(UStaticMesh* StaticMesh)
{
	UStaticMeshComponent* const* FoundStaticMeshComponent = StaticMeshComponents.Find(StaticMesh);
	if(FoundStaticMeshComponent)
	{
		StaticMeshComponents.Remove(StaticMesh);

		UStaticMeshComponent* StaticMeshComponent = *FoundStaticMeshComponent;
		if(StaticMeshComponent)
		{
			StaticMeshComponent->DetachFromParent();
			StaticMeshComponent->UnregisterComponent();
			StaticMeshComponent->DestroyComponent();
		}
	}
}
