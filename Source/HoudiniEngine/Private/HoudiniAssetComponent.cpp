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


UScriptStruct*
UHoudiniAssetComponent::ScriptStructColor = nullptr;


// Define accessor for UObjectBase::SetClass private method.
struct FObjectBaseAccess
{
	typedef void (UObjectBase::*type)(UClass*);
};

HOUDINI_PRIVATE_PATCH(FObjectBaseAccess, UObjectBase::SetClass);



UHoudiniAssetComponent::UHoudiniAssetComponent(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	HoudiniAsset(nullptr),
	AssetId(-1),
	bIsNativeComponent(false),
	bIsPreviewComponent(false),
	bAsyncResourceReleaseHasBeenStarted(false)
{
	// Create a generic bounding volume.
	HoudiniMeshSphereBounds = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));

	// Set component properties.
	Mobility = EComponentMobility::Movable;
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bGenerateOverlapEvents = false;

	// This component requires render update.
	bNeverNeedsRenderUpdate = false;

	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID.Invalidate();

	// Zero scratch space.
	FMemory::Memset(ScratchSpaceBuffer, 0x0, HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE);

	// Create temporary geometry.
	FHoudiniEngineUtils::GetHoudiniLogoGeometry(HoudiniMeshTriangles, HoudiniMeshSphereBounds);
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
				// Retrieve asset associated with this component.
				UHoudiniAsset* HoudiniAsset = HoudiniAssetComponent->GetHoudiniAsset();
				if(HoudiniAsset)
				{
					// Manually add a reference to Houdini asset from this component.
					Collector.AddReferencedObject(HoudiniAsset, InThis);
				}

				// Propagate referencing request to all geos.
				for(TArray<FHoudiniAssetObjectGeo*>::TIterator Iter = HoudiniAssetComponent->HoudiniAssetObjectGeos.CreateIterator(); Iter; ++Iter)
				{
					FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = *Iter;
					HoudiniAssetObjectGeo->AddReferencedObjects(Collector);
				}
			}
		}
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
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


TWeakObjectPtr<AHoudiniAssetActor>
UHoudiniAssetComponent::GetHoudiniAssetActorOwner() const
{
	return HoudiniAssetActorOwner;
}


void
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("Setting asset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	UHoudiniAsset* Asset = nullptr;
	TWeakObjectPtr<AHoudiniAssetActor> HoudiniAssetActor = GetHoudiniAssetActorOwner();
	check(HoudiniAssetActor.IsValid());

	// If it is the same asset, do nothing.
	if(InHoudiniAsset == HoudiniAsset)
	{
		return;
	}

	HoudiniAsset = InHoudiniAsset;
	bIsPreviewComponent = HoudiniAssetActor->IsUsedForPreview();

	if(!InHoudiniAsset->DoesPreviewGeometryContainHoudiniLogo())
	{
		// If asset contains non logo geometry, retrieve it and use it.
		InHoudiniAsset->RetrievePreviewGeometry(HoudiniMeshTriangles);

		// Update rendering information.
		UpdateRenderingInformation();
	}

	if(!bIsPreviewComponent)
	{
		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		// Create asset instantiation task object and submit it for processing.
		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetInstantiation, HapiGUID);
		Task.Asset = InHoudiniAsset;
		Task.ActorName = HoudiniAssetActor->GetActorLabel();
		FHoudiniEngine::Get().AddTask(Task);

		// Start ticking - this will poll the cooking system for completion.
		StartHoudiniTicking();
	}
}


void
UHoudiniAssetComponent::AssignUniqueActorLabel()
{
	if(-1 != AssetId)
	{
		TWeakObjectPtr<AHoudiniAssetActor> HoudiniAssetActor = GetHoudiniAssetActorOwner();
		if(HoudiniAssetActor.IsValid())
		{
			FString UniqueName;
			if(FHoudiniEngineUtils::GetHoudiniAssetName(AssetId, UniqueName))
			{
				GEditor->SetActorLabelUnique(HoudiniAssetActor.Get(), UniqueName);
			}
		}
	}
}


void
UHoudiniAssetComponent::ClearGeos()
{
	for(TArray<FHoudiniAssetObjectGeo*>::TIterator Iter = HoudiniAssetObjectGeos.CreateIterator(); Iter; ++Iter)
	{
		delete(*Iter);
	}

	HoudiniAssetObjectGeos.Empty();
}


bool
UHoudiniAssetComponent::ContainsGeos() const
{
	return (HoudiniAssetObjectGeos.Num() > 0);
}


void
UHoudiniAssetComponent::StartHoudiniTicking()
{
	// If we have no timer delegate spawned for this preview component, spawn one.
	if(!TimerDelegate.IsBound())
	{
		TimerDelegate = FTimerDelegate::CreateUObject(this, &UHoudiniAssetComponent::TickHoudiniComponent);

		// We need to register delegate with the timer system.
		static const float TickTimerDelay = 0.25f;
		GEditor->GetTimerManager()->SetTimer(TimerDelegate, TickTimerDelay, true);
	}
}


void
UHoudiniAssetComponent::StopHoudiniTicking()
{
	if(TimerDelegate.IsBound())
	{
		GEditor->GetTimerManager()->ClearTimer(TimerDelegate);
		TimerDelegate.Unbind();
	}
}


void
UHoudiniAssetComponent::TickHoudiniComponent()
{
	FHoudiniEngineTaskInfo TaskInfo;
	bool bStopTicking = false;

	// Retrieve the owner actor of this component.
	TWeakObjectPtr<AHoudiniAssetActor> HoudiniAssetActor = GetHoudiniAssetActorOwner();
	check(HoudiniAssetActor.IsValid());

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
					Info.FadeOutDuration = 2.0f;
					Info.ExpireDuration = 2.0f;

					TSharedPtr<FSlateDynamicImageBrush> HoudiniBrush = FHoudiniEngine::Get().GetHoudiniLogoBrush();
					if(HoudiniBrush.IsValid())
					{
						Info.Image = HoudiniBrush.Get();
					}

					NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
				}
			}

			switch(TaskInfo.TaskState)
			{
				case EHoudiniEngineTaskState::FinishedInstantiation:
				case EHoudiniEngineTaskState::FinishedCooking:
				{
					if(-1 != TaskInfo.AssetId)
					{
						// Set new asset id.
						SetAssetId(TaskInfo.AssetId);

						// Assign unique actor label based on asset name.
						AssignUniqueActorLabel();

						if(FHoudiniEngineUtils::GetAssetGeometry(TaskInfo.AssetId, HoudiniMeshTriangles, HoudiniMeshSphereBounds))
						{
							// We need to patch component RTTI to reflect properties for this component.
							ReplaceClassInformation();

							// Get current asset.
							UHoudiniAsset* CurrentHoudiniAsset = GetHoudiniAsset();

							// See if asset contains Houdini logo geometry, if it does we can update it.
							if(CurrentHoudiniAsset && CurrentHoudiniAsset->DoesPreviewGeometryContainHoudiniLogo())
							{
								CurrentHoudiniAsset->SetPreviewGeometry(HoudiniMeshTriangles);

								// We need to find corresponding preview component.
								for(TObjectIterator<UHoudiniAssetComponent> It; It; ++It)
								{
									UHoudiniAssetComponent* HoudiniAssetComponent = *It;

									// Skip ourselves.
									if(HoudiniAssetComponent == this)
									{
										continue;
									}

									if(HoudiniAssetComponent->HoudiniAsset && HoudiniAssetComponent->HoudiniAsset == CurrentHoudiniAsset)
									{
										// Update preview actor geometry with new data.
										HoudiniAssetComponent->HoudiniMeshTriangles = HoudiniMeshTriangles;
										HoudiniAssetComponent->UpdateRenderingInformation();

										break;
									}
								}
							}

							// Update properties panel.
							UpdateEditorProperties();

							// Clear rendering resources used by geos.
							ReleaseRenderingResources();

							// Delete all existing geo objects (this will also delete their geo parts).
							ClearGeos();

							// Construct new objects (asset objects and asset object parts).
							FHoudiniEngineUtils::ConstructGeos(AssetId, HoudiniAssetObjectGeos);

							// Create all rendering resources.
							CreateRenderingResources();

							// Generate necessary materials.
							CreateComponentMaterials();

							// Need to update rendering information.
							UpdateRenderingInformation();
						}
						else
						{
							HOUDINI_LOG_MESSAGE(TEXT("Failed geometry extraction after asset instantiation."));
						}
					}
					else
					{
						HOUDINI_LOG_MESSAGE(TEXT("Received invalid asset id."));
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
					break;
				}

				case EHoudiniEngineTaskState::Aborted:
				case EHoudiniEngineTaskState::FinishedInstantiationWithErrors:
				case EHoudiniEngineTaskState::FinishedCookingWithErrors:
				{
					HOUDINI_LOG_MESSAGE(TEXT("Failed asset instantiation."));

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

	if(!HapiGUID.IsValid() && (ChangedProperties.Num() > 0))
	{
		// If we are not cooking and we have property changes queued up.

		// We need to set all parameter values which have changed.
		SetChangedParameterValues();

		// Remove all processed parameters.
		ChangedProperties.Empty();

		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		// Create asset instantiation task object and submit it for processing.
		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, HapiGUID);
		Task.ActorName = HoudiniAssetActor->GetActorLabel();
		Task.AssetComponent = this;
		FHoudiniEngine::Get().AddTask(Task);

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
	TWeakObjectPtr<AHoudiniAssetActor> HoudiniAssetActor = GetHoudiniAssetActorOwner();
	if(HoudiniAssetActor.IsValid())
	{
		// Manually reselect the actor - this will cause details panel to be updated and force our 
		// property changes to be picked up by the UI.
		GEditor->SelectActor(HoudiniAssetActor.Get(), true, true);

		// Notify the editor about selection change.
		GEditor->NoteSelectionChange();
	}
}


FBoxSphereBounds
UHoudiniAssetComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return HoudiniMeshSphereBounds;
}


int32
UHoudiniAssetComponent::GetNumMaterials() const
{
	return 1;
}


void
UHoudiniAssetComponent::CreateRenderingResources()
{
	for(TArray<FHoudiniAssetObjectGeo*>::TIterator Iter = HoudiniAssetObjectGeos.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = *Iter;
		HoudiniAssetObjectGeo->CreateRenderingResources();
	}
}


void
UHoudiniAssetComponent::ReleaseRenderingResources()
{
	for(TArray<FHoudiniAssetObjectGeo*>::TIterator Iter = HoudiniAssetObjectGeos.CreateIterator(); Iter; ++Iter)
	{
		FHoudiniAssetObjectGeo* HoudiniAssetObjectGeo = *Iter;
		HoudiniAssetObjectGeo->ReleaseRenderingResources();
	}

	// Insert a fence to signal when these commands completed.
	ReleaseResourcesFence.BeginFence();
	bAsyncResourceReleaseHasBeenStarted = true;

	// Wait for fence to complete.
	ReleaseResourcesFence.Wait();
	bAsyncResourceReleaseHasBeenStarted = false;
}


void
UHoudiniAssetComponent::CreateComponentMaterials()
{
	// Get the label of the owner actor.
	TWeakObjectPtr<AHoudiniAssetActor> HoudiniAssetActor = GetHoudiniAssetActorOwner();
	check(HoudiniAssetActor.IsValid());

	const FString& HoudiniAssetActorLabel = HoudiniAssetActor->GetActorLabel();

	// Get the outermost package of the asset.
	UPackage* HoudiniAssetPackage = HoudiniAsset->GetOutermost();

	// Create generated material name based on actor.
	FString MaterialName = FString::Printf(TEXT("%s_material"), *HoudiniAssetActorLabel);

	// Create package name for material.
	FString PackageName = FPackageName::GetLongPackagePath(HoudiniAssetPackage->GetName()) + TEXT("/") + MaterialName;

	// Make sure package name is valid.
	PackageName = PackageTools::SanitizePackageName(PackageName);

	// Check if package exists already.
	UPackage* Package = FindPackage(NULL, *PackageName);
	if(!Package)
	{
		// Create the package if it does not exist.
		Package = CreatePackage(NULL, *PackageName);
	}

	UMaterial* HoudiniGeneratedMaterial = nullptr;

	// Check if we need to create a new material object.
	if(!GetMaterial(0))
	{
		// Create a new material factory to create our material asset.
		UMaterialFactoryNew* MaterialFactory = new UMaterialFactoryNew(FPostConstructInitializeProperties());











		HAPI_AssetInfo AssetInfo;
		HAPI_GetAssetInfo(AssetId, &AssetInfo);

		HAPI_PartInfo PartInfo;
		HAPI_GetPartInfo(AssetId, 0, 0, 0, &PartInfo);

		// Load textures.
		HAPI_MaterialInfo MaterialInfo;
		HAPI_GetMaterialOnPart(AssetId, 0, 0, 0, &MaterialInfo);

		HAPI_NodeInfo NodeInfo;
		HAPI_GetNodeInfo(MaterialInfo.nodeId, &NodeInfo);

		std::vector<HAPI_ParmInfo> NodeParams;
		NodeParams.resize(NodeInfo.parmCount);
		HAPI_GetParameters(NodeInfo.id, &NodeParams[0], 0, NodeInfo.parmCount);

		int TexturesLoaded = 0;

		HAPI_Result Result;


		for(int ParmIdx = 0; ParmIdx < NodeInfo.parmCount; ++ParmIdx)
		{
			HAPI_ParmInfo& NodeParmInfo = NodeParams[ParmIdx];
			HAPI_StringHandle NodeParmHandle = NodeParmInfo.nameSH;

			int NodeParmNameLength = 0;
			HAPI_GetStringBufLength(NodeParmHandle, &NodeParmNameLength);

			std::vector<char> NodeParmName;
			NodeParmName.reserve(NodeParmNameLength + 1);
			NodeParmName[NodeParmNameLength] = '\0';
			HAPI_GetString(NodeParmHandle, &NodeParmName[0], NodeParmNameLength);

			if(!strncmp(&NodeParmName[0], "map", 3) || !strncmp(&NodeParmName[0], "ogl_tex1", 8) || !strncmp(&NodeParmName[0], "baseColorMap", 12))
			{
				Result = HAPI_RenderTextureToImage(AssetInfo.id, MaterialInfo.id, NodeParmInfo.id);

				if(Result != HAPI_RESULT_SUCCESS) 
				{
					continue;
				}

				//extractHoudiniImageToTexture( material_info, folder_path, "C A" );
				HAPI_ImageInfo ImageInfo;
				Result = HAPI_GetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);

				ImageInfo.dataFormat = HAPI_IMAGE_DATA_INT8;
				ImageInfo.interleaved = true;
				ImageInfo.packing = HAPI_IMAGE_PACKING_RGBA;

				HAPI_SetImageInfo(MaterialInfo.assetId, MaterialInfo.id, &ImageInfo);

				int ImageBufferSize = 0;
				HAPI_ExtractImageToMemory(MaterialInfo.assetId, MaterialInfo.id, HAPI_RAW_FORMAT_NAME, "C A", &ImageBufferSize);

				std::vector<char> ImageBuffer;
				ImageBuffer.reserve(ImageBufferSize);
				HAPI_GetImageMemoryBuffer(MaterialInfo.assetId, MaterialInfo.id, &ImageBuffer[0], ImageBufferSize);

				// Create a new generated material asset.
				HoudiniGeneratedMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *MaterialName, RF_Standalone | RF_Public, NULL, GWarn);
				if(HoudiniGeneratedMaterial)
				{
					// Notify asset registry about creation of a new material asset.
					FAssetRegistryModule::AssetCreated(HoudiniGeneratedMaterial);

					// Set the dirty flag so this package will get saved later.
					Package->SetDirtyFlag(true);

					// Perform notifications regarding material changes.
					//HoudiniGeneratedMaterial->PreEditChange(nullptr);
					//HoudiniGeneratedMaterial->PostEditChange();

					//SetMaterial(0, HoudiniGeneratedMaterial);

					HoudiniGeneratedMaterial->TwoSided = false;
					HoudiniGeneratedMaterial->SetShadingModel(MSM_DefaultLit);
				}


				// Create diffuse texture.
				UTexture2D* DiffuseTexture = UTexture2D::CreateTransient(ImageInfo.xRes, ImageInfo.yRes, PF_B8G8R8A8);

				// Lock texture for modification.
				uint8* MipData = static_cast<uint8*>(DiffuseTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

				// Create base map.
				uint8* DestPtr = NULL;
				const FColor* SrcPtr = NULL;
				uint32 SrcWidth = ImageInfo.xRes;
				uint32 SrcHeight = ImageInfo.yRes;
				const char* SrcData = &ImageBuffer[0];

				for(uint32 y = 0; y < SrcHeight; y++)
				{
					DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];

					for(uint32 x = 0; x < SrcWidth; x++)
					{
						uint32 DataOffset = y * SrcWidth * 4 + x * 4;

						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 0); //B
						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 1); //G
						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 2); //R
						*DestPtr++ = *(uint8*)(SrcData + DataOffset + 3); //A
						//*DestPtr++ = 0xFF; //A
					}
				}

				// Unlock the texture.
				DiffuseTexture->PlatformData->Mips[0].BulkData.Unlock();
				DiffuseTexture->UpdateResource();




				// Assign texture to material.
				UMaterialExpressionTextureSample* Expression = ConstructObject<UMaterialExpressionTextureSample>(UMaterialExpressionTextureSample::StaticClass(), HoudiniGeneratedMaterial);
				HoudiniGeneratedMaterial->Expressions.Add(Expression);
				HoudiniGeneratedMaterial->DiffuseColor.Expression = Expression;
				Expression->Texture = DiffuseTexture;


				// Perform notifications regarding material changes.
				HoudiniGeneratedMaterial->PreEditChange(nullptr);
				HoudiniGeneratedMaterial->PostEditChange();

				SetMaterial(0, HoudiniGeneratedMaterial);

				for (TArray<FHoudiniAssetObjectGeo*>::TIterator IterGeo = HoudiniAssetObjectGeos.CreateIterator(); IterGeo; ++IterGeo)
				{
					FHoudiniAssetObjectGeo* Geo = *IterGeo;
					Geo->Material = HoudiniGeneratedMaterial;

					/*
					for (TArray<FHoudiniAssetObjectGeoPart*>::TIterator IterGeoPart = Geo->HoudiniAssetObjectGeoParts.CreateIterator(); IterGeoPart; ++IterGeoPart)
					{
						FHoudiniAssetObjectGeoPart* Part = *IterGeoPart;
						Part->Material = HoudiniGeneratedMaterial;
					}
					*/
				}
			}
		}
	}
	else
	{
		HoudiniGeneratedMaterial = (UMaterial*) GetMaterial(0);
	}


	if(HoudiniGeneratedMaterial)
	{
		for (TArray<FHoudiniAssetObjectGeo*>::TIterator IterGeo = HoudiniAssetObjectGeos.CreateIterator(); IterGeo; ++IterGeo)
		{
			FHoudiniAssetObjectGeo* Geo = *IterGeo;
			Geo->Material = HoudiniGeneratedMaterial;

			/*
			for (TArray<FHoudiniAssetObjectGeoPart*>::TIterator IterGeoPart = Geo->HoudiniAssetObjectGeoParts.CreateIterator(); IterGeoPart; ++IterGeoPart)
			{
				FHoudiniAssetObjectGeoPart* Part = *IterGeoPart;
				Part->Material = HoudiniGeneratedMaterial;
			}
			*/
		}
	}













		/*
		// Create a new generated material asset.
		UMaterial* HoudiniGeneratedMaterial = (UMaterial*) MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *MaterialName, RF_Standalone | RF_Public, NULL, GWarn);
		if(HoudiniGeneratedMaterial)
		{
			// Notify asset registry about creation of a new material asset.
			FAssetRegistryModule::AssetCreated(HoudiniGeneratedMaterial);

			// Set the dirty flag so this package will get saved later.
			Package->SetDirtyFlag(true);

			// Perform notifications regarding material changes.
			HoudiniGeneratedMaterial->PreEditChange(nullptr);
			HoudiniGeneratedMaterial->PostEditChange();

			SetMaterial(0, HoudiniGeneratedMaterial);
		}
		*/
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

	if(ContainsGeos())
	{
		Proxy = new FHoudiniMeshSceneProxy(this);
	}

	return Proxy;
}


void
UHoudiniAssetComponent::OnComponentDestroyed()
{
	HOUDINI_LOG_MESSAGE(TEXT("Destroying component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

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
	if(-1 != AssetId)
	{
		// Generate GUID for our new task.
		HapiGUID = FGuid::NewGuid();

		// Create asset deletion task object and submit it for processing.
		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetDeletion, HapiGUID);
		Task.AssetId = AssetId;
		FHoudiniEngine::Get().AddTask(Task);

		// Reset asset id.
		AssetId = -1;
	}

	Super::OnComponentDestroyed();
}


void
UHoudiniAssetComponent::BeginDestroy()
{
	Super::BeginDestroy();

	// Before releasing resources make sure we do not have scene proxy active.
	check(!SceneProxy);

	// Now we can release rendering resources.
	ReleaseRenderingResources();
}


void
UHoudiniAssetComponent::FinishDestroy()
{
	Super::FinishDestroy();

	{
		check(ReleaseResourcesFence.IsFenceComplete());
		ClearGeos();
	}
}


bool
UHoudiniAssetComponent::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.IsFenceComplete();
}


void
UHoudiniAssetComponent::ReplacePropertyOffset(UProperty* Property, int Offset)
{
	// We need this bit of magic in order to replace the private field.
	*(int*)((char*)&Property->RepNotifyFunc + sizeof(FName)) = Offset;
}


void
UHoudiniAssetComponent::ReplaceClassInformation()
{
	UClass* PatchedClass = nullptr;
	UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

	// If RTTI has not been previously patched, we need to do so.
	if(GetClass() == ClassOfUHoudiniAssetComponent)
	{
		// Construct unique name for this class.
		FString PatchedClassName = FString::Printf(TEXT("%s_%d"), *GetClass()->GetName(), AssetId);

		// Create new class instance.
		//static const EObjectFlags PatchedClassFlags = RF_Public | RF_Standalone | RF_Transient | RF_Native | RF_RootSet;
		static const EObjectFlags PatchedClassFlags = RF_Public | RF_Standalone;

		// Construct the new class instance.
		PatchedClass = ConstructObject<UClass>(UClass::StaticClass(), GetOutermost(), FName(*PatchedClassName), PatchedClassFlags, ClassOfUHoudiniAssetComponent, true);

		// Use same class flags as the original class.
		PatchedClass->ClassFlags = UHoudiniAssetComponent::StaticClassFlags;

		// Use same class cast flags as the original class (these are used for quick casting between common types).
		PatchedClass->ClassCastFlags = UHoudiniAssetComponent::StaticClassCastFlags();

		// Use same class configuration name.
		PatchedClass->ClassConfigName = UHoudiniAssetComponent::StaticConfigName();

		// Reuse class default object (this is essentially a template object used for construction).
		PatchedClass->ClassDefaultObject = GetClass()->ClassDefaultObject;

		// We will reuse the same constructor as nothing has really changed.
		PatchedClass->ClassConstructor = ClassOfUHoudiniAssetComponent->ClassConstructor;

		// Register our own reference counting registration.
		PatchedClass->ClassAddReferencedObjects = UHoudiniAssetComponent::AddReferencedObjects;

		// Minimum class alignment does not change.
		PatchedClass->MinAlignment = ClassOfUHoudiniAssetComponent->MinAlignment;

		// Properties size does not change as we use the same fixed size buffer.
		PatchedClass->PropertiesSize = ClassOfUHoudiniAssetComponent->PropertiesSize;

		//PatchedClass->SetSuperStruct(UMeshComponent::StaticClass()->GetSuperStruct());
		PatchedClass->SetSuperStruct(ClassOfUHoudiniAssetComponent->GetSuperStruct());

		// List of replication records.
		PatchedClass->ClassReps = ClassOfUHoudiniAssetComponent->ClassReps;

		// List of network relevant fields (properties and functions).
		PatchedClass->NetFields = ClassOfUHoudiniAssetComponent->NetFields;

		// Reference token stream used by real time garbage collector.
		PatchedClass->ReferenceTokenStream = ClassOfUHoudiniAssetComponent->ReferenceTokenStream;

		// This class's native functions.
		PatchedClass->NativeFunctionLookupTable = ClassOfUHoudiniAssetComponent->NativeFunctionLookupTable;

		// Now that we've filled all necessary fields, patch class information.
		ReplaceClassObject(PatchedClass);
	}
	else
	{
		// Otherwise we need to destroy and recreate all properties.
		PatchedClass = GetClass();
		RemoveClassProperties(PatchedClass);
	}

	// Insert necessary properties.
	ReplaceClassProperties(PatchedClass);
}


void
UHoudiniAssetComponent::ReplaceClassObject(UClass* ClassObjectNew)
{
	HOUDINI_PRIVATE_CALL(FObjectBaseAccess, UObjectBase, ClassObjectNew);
}


void
UHoudiniAssetComponent::RemoveClassProperties(UClass* ClassInstance)
{
	UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

	UProperty* IterProperty = ClassInstance->PropertyLink;
	while(IterProperty && IterProperty != ClassOfUHoudiniAssetComponent->PropertyLink)
	{
		UProperty* Property = IterProperty;
		IterProperty = IterProperty->PropertyLinkNext;

		//Property->ClearFlags(RF_Native | RF_RootSet);
		Property->Next = nullptr;
		Property->PropertyLinkNext = nullptr;
	}

	// Do not need to update / remove / delete children as those will be by construction same as properties.
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
	std::vector<HAPI_StringHandle> ParmStringFloats;
	std::vector<char> ParmName;
	std::vector<char> ParmLabel;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), false);
	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetNodeInfo(AssetInfo.nodeId, &NodeInfo), false);

	//FIXME: everything hard coded to 0 for now.
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
	ParmStringFloats.resize(NodeInfo.parmStringValueCount);
	if(NodeInfo.parmStringValueCount > 0)
	{
		HOUDINI_CHECK_ERROR_RETURN(HAPI_GetParmStringValues(AssetInfo.nodeId, true, &ParmStringFloats[0], 0, NodeInfo.parmStringValueCount), false);
	}

	// We need to insert new properties and new children in the beginning of single link list.
	// This way properties and children from the original class can be reused and will not have
	// their next pointers altered.
	UProperty* PropertyFirst = nullptr;
	UProperty* PropertyLast = PropertyFirst;

	UField* ChildFirst = nullptr;
	UField* ChildLast = ChildFirst;

	uint32 ValuesOffsetStart = offsetof(UHoudiniAssetComponent, ScratchSpaceBuffer);
	uint32 ValuesOffsetEnd = ValuesOffsetStart;

	for(int idx = 0; idx < NodeInfo.parmCount; ++idx)
	{
		// Retrieve param info at this index.
		const HAPI_ParmInfo& ParmInfoIter = ParmInfo[idx];

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
			{
				break;
			}

			case HAPI_PARMTYPE_STRING:
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
		switch(ParmInfoIter.type)
		{
			case HAPI_PARMTYPE_INT:
			{
				Property = CreatePropertyInt(ClassInstance, ParmNameConverted, ParmInfoIter.size, &ParmValuesIntegers[ParmInfoIter.intValuesIndex], ValuesOffsetEnd);
				break;
			}

			case HAPI_PARMTYPE_FLOAT:
			{
				Property = CreatePropertyFloat(ClassInstance, ParmNameConverted, ParmInfoIter.size, &ParmValuesFloats[ParmInfoIter.floatValuesIndex], ValuesOffsetEnd);
				break;
			}

			case HAPI_PARMTYPE_TOGGLE:
			{
				Property = CreatePropertyToggle(ClassInstance, ParmNameConverted, ParmInfoIter.size, &ParmValuesIntegers[ParmInfoIter.intValuesIndex], ValuesOffsetEnd);
				break;
			}

			case HAPI_PARMTYPE_COLOR:
			{
				Property = CreatePropertyColor(ClassInstance, ParmNameConverted, ParmInfoIter.size, &ParmValuesFloats[ParmInfoIter.floatValuesIndex], ValuesOffsetEnd);
				break;
			}
			case HAPI_PARMTYPE_STRING:
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
	}

	UClass* ClassOfUHoudiniAssetComponent = UHoudiniAssetComponent::StaticClass();

	if(PropertyFirst)
	{
		ClassInstance->PropertyLink = PropertyFirst;
		PropertyLast->PropertyLinkNext = ClassOfUHoudiniAssetComponent->PropertyLink;
	}

	if(ChildFirst)
	{
		ClassInstance->Children = ChildFirst;
		ChildLast->Next = ClassOfUHoudiniAssetComponent->Children;
	}

	return true;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyColor(UClass* ClassInstance, const FName& Name, int Count, const float* Value, uint32& Offset)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// Color must have 3 or 4 fields.
	if(Count < 3)
	{
		return nullptr;
	}

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

	UStructProperty* Property = FindObject<UStructProperty>(ClassInstance, *Name.ToString(), false);
	if(!Property)
	{
		Property = NewNamedObject<UStructProperty>(ClassInstance, Name, PropertyObjectFlags);
		Property->Struct = UHoudiniAssetComponent::ScriptStructColor;
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;

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
	*Boundary = ConvertedColor;
	
	// Increment offset for next property.
	Offset = Offset + sizeof(FColor);

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyInt(UClass* ClassInstance, const FName& Name, int Count, const int32* Value, uint32& Offset)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;
	static const uint64 PropertyFlags =  UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	UIntProperty* Property = FindObject<UIntProperty>(ClassInstance, *Name.ToString(), false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UIntProperty>(ClassInstance, Name, PropertyObjectFlags);
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	int* Boundary = ComputeOffsetAlignmentBoundary<int>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	for(int Index = 0; Index < Count; ++Index)
	{
		*Boundary = *(Value + Index);
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(int) * Count;

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyFloat(UClass* ClassInstance, const FName& Name, int Count, const float* Value, uint32& Offset)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;
	static const uint64 PropertyFlags =  UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	UFloatProperty* Property = FindObject<UFloatProperty>(ClassInstance, *Name.ToString(), false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UFloatProperty>(ClassInstance, Name, PropertyObjectFlags);
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	float* Boundary = ComputeOffsetAlignmentBoundary<float>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	for(int Index = 0; Index < Count; ++Index)
	{
		*Boundary = *(Value + Index);
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(float) * Count;

	return Property;
}


UProperty*
UHoudiniAssetComponent::CreatePropertyToggle(UClass* ClassInstance, const FName& Name, int Count, const int32* bValue, uint32& Offset)
{
	static const EObjectFlags PropertyObjectFlags = RF_Public | RF_Transient;
	static const uint64 PropertyFlags = UINT64_C(69793219077);

	// Ignore parameters with size zero.
	if(!Count)
	{
		return nullptr;
	}

	UBoolProperty* Property = FindObject<UBoolProperty>(ClassInstance, *Name.ToString(), false);
	if(!Property)
	{
		// Property does not exist, we need to create it.
		Property = NewNamedObject<UBoolProperty>(ClassInstance, Name, PropertyObjectFlags);
	}

	Property->PropertyLinkNext = nullptr;
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniProperties"));
	Property->PropertyFlags = PropertyFlags;
	Property->SetBoolSize(sizeof(bool), true);

	// Set property size. Larger than one indicates array.
	Property->ArrayDim = Count;

	// We need to compute proper alignment for this type.
	bool* Boundary = ComputeOffsetAlignmentBoundary<bool>(Offset);
	Offset = (const char*) Boundary - (const char*) this;

	// Need to patch offset for this property.
	ReplacePropertyOffset(Property, Offset);

	// Write property data to which it refers by offset.
	for(int Index = 0; Index < Count; ++Index)
	{
		*Boundary = (*(bValue + Index) != 0);
	}

	// Increment offset for next property.
	Offset = Offset + sizeof(bool) * Count;

	return Property;
}


void
UHoudiniAssetComponent::SetChangedParameterValues()
{
	HAPI_Result Result = HAPI_RESULT_SUCCESS;
	HAPI_AssetInfo AssetInfo;

	HOUDINI_CHECK_ERROR_RETURN(HAPI_GetAssetInfo(AssetId, &AssetInfo), void());

	for(TSet<UProperty*>::TIterator Iter(ChangedProperties); Iter; ++Iter)
	{
		UProperty* Property = *Iter;

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
			continue;
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
}


void
UHoudiniAssetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Retrieve property which changed. Property field is a property which is being modified. MemberProperty
	// field is a property which contains the modified property (for example if modified property is a member of a
	// struct).
	UProperty* Property = PropertyChangedEvent.MemberProperty;
	UProperty* PropertyChild = PropertyChangedEvent.Property;

	// Retrieve property category.
	static const FString CategoryHoudiniAsset = TEXT("HoudiniProperties");
	const FString& Category = Property->GetMetaData(TEXT("Category"));

	if(Category != CategoryHoudiniAsset)
	{
		// This property is not in category we are interested in, just jump out.
		return;
	}

	HOUDINI_LOG_MESSAGE(TEXT("PostEditChangeProperty, Property = 0x%0.8p, PropertyChild = 0x%0.8p"), Property, PropertyChild);

	if(EPropertyChangeType::Interactive == PropertyChangedEvent.ChangeType)
	{
		if(UStructProperty::StaticClass() == Property->GetClass())
		{
			UStructProperty* StructProperty = Cast<UStructProperty>(Property);

			if(UHoudiniAssetComponent::ScriptStructColor == StructProperty->Struct)
			{
				// Ignore interactive events for color properties.
				return;
			}
		}
	}

	// Add changed property to the set of changes.
	ChangedProperties.Add(Property);

	// Start ticking (if we are ticking already, this will be ignored).
	StartHoudiniTicking();
}


void
UHoudiniAssetComponent::OnRegister()
{
	Super::OnRegister();
	HOUDINI_LOG_MESSAGE(TEXT("Registering component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	// Make sure we have a Houdini asset to operate with.
	if(!HoudiniAsset)
	{
		return;
	}

	TWeakObjectPtr<AHoudiniAssetActor> HoudiniAssetActor = GetHoudiniAssetActorOwner();
	if(!HoudiniAssetActor.IsValid())
	{
		return;
	}

	if(bIsNativeComponent)
	{
		// This is a native component ~ belonging to a c++ actor.

		if(bIsPreviewComponent)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Native::OnRegister, Preview actor"));
		}
		else
		{
			HOUDINI_LOG_MESSAGE(TEXT("Native::OnRegister, Non-preview actor"));
		}
	}
	else
	{
		// This is a dynamic component ~ part of blueprint.
		HOUDINI_LOG_MESSAGE(TEXT("Dynamic::OnRegister"));
	}
}


void
UHoudiniAssetComponent::OnUnregister()
{
	Super::OnUnregister();
	HOUDINI_LOG_MESSAGE(TEXT("Unregistering component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void
UHoudiniAssetComponent::OnComponentCreated()
{
	// This event will only be fired for native Actor and native Component.
	Super::OnComponentCreated();
	HOUDINI_LOG_MESSAGE(TEXT("Creating component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


FName
UHoudiniAssetComponent::GetComponentInstanceDataType() const
{
	// Called before we throw away components during RerunConstructionScripts, to cache any data we wish to persist across that operation.

	return Super::GetComponentInstanceDataType();
	HOUDINI_LOG_MESSAGE(TEXT("Requesting data for caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void
UHoudiniAssetComponent::ApplyComponentInstanceData(TSharedPtr<class FComponentInstanceDataBase> ComponentInstanceData)
{
	// Called after we create new components during RerunConstructionScripts, to optionally apply any data backed up during GetComponentInstanceData.

	Super::ApplyComponentInstanceData(ComponentInstanceData);
	HOUDINI_LOG_MESSAGE(TEXT("Restoring data from caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}
