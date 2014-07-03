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
	HoudiniPreviewAsset(nullptr),
	AssetId(-1),
	bIsNativeComponent(false),
	bIsPreviewComponent(false)
{
	// Create a generic bounding volume.
	HoudiniMeshSphereBounds = FBoxSphereBounds(FBox(-FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX, FVector(1.0f, 1.0f, 1.0f) * HALF_WORLD_MAX));

	// Make an invalid GUID, since we do not have any cooking requests.
	HapiGUID.Invalidate();

	// Zero scratch space.
	FMemory::Memset(ScratchSpaceBuffer, 0x0, HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE);

	// Create temporary geometry.
	FHoudiniEngineUtils::GetHoudiniLogoGeometry(HoudiniMeshTriangles, HoudiniMeshSphereBounds);
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
	if(HoudiniAsset)
	{
		return HoudiniAsset;
	}
	else if(HoudiniPreviewAsset)
	{
		return HoudiniPreviewAsset;
	}

	return nullptr;
}


void
UHoudiniAssetComponent::OnRep_HoudiniAsset(UHoudiniAsset* OldHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("OnRep_HoudiniAsset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	if(OldHoudiniAsset != HoudiniAsset)
	{
		// We have to force a call to SetHoudiniAsset with a new HoudiniAsset.
		UHoudiniAsset* NewHoudiniAsset = HoudiniAsset;
		HoudiniAsset = nullptr;

		SetHoudiniAsset(NewHoudiniAsset);
	}
}


void
UHoudiniAssetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHoudiniAssetComponent, HoudiniAsset);
}


void
UHoudiniAssetComponent::SetHoudiniAsset(UHoudiniAsset* InHoudiniAsset)
{
	HOUDINI_LOG_MESSAGE(TEXT("Setting asset, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);

	UHoudiniAsset* Asset = nullptr;
	AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(GetOwner());
	check(HoudiniAssetActor);

	if(HoudiniAssetActor->IsUsedForPreview())
	{
		// If it is the same asset, do nothing.
		if(InHoudiniAsset == HoudiniPreviewAsset)
		{
			return;
		}

		HoudiniPreviewAsset = InHoudiniAsset;
		bIsPreviewComponent = true;
	}
	else
	{
		// If it is the same asset, do nothing.
		if(InHoudiniAsset == HoudiniAsset)
		{
			return;
		}

		HoudiniAsset = InHoudiniAsset;
	}

	if(!InHoudiniAsset->DoesPreviewGeometryContainHoudiniLogo())
	{
		// If asset contains non logo geometry, retrieve it and use it.
		InHoudiniAsset->RetrievePreviewGeometry(HoudiniMeshTriangles);

		// Update rendering information.
		UpdateRenderingInformation();
	}

	if(!HoudiniAssetActor->IsUsedForPreview())
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
					if (HoudiniBrush.IsValid())
					{
						Info.Image = HoudiniBrush.Get();
					}

					NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
				}
			}

			switch(TaskInfo.TaskState)
			{
				case EHoudiniEngineTaskState::Finished:
				{
					if(-1 != TaskInfo.AssetId)
					{
						// Set new asset id.
						SetAssetId(TaskInfo.AssetId);

						if(FHoudiniEngineUtils::GetAssetGeometry(TaskInfo.AssetId, HoudiniMeshTriangles, HoudiniMeshSphereBounds))
						{
							// We need to patch component RTTI to reflect properties for this component.
							ReplaceClassInformation();

							// Need to update rendering information.
							UpdateRenderingInformation();

							// See if asset contains Houdini logo geometry, if it does we can update it.
							UHoudiniAsset* CurrentHoudiniAsset = GetHoudiniAsset();
							if(CurrentHoudiniAsset && CurrentHoudiniAsset->DoesPreviewGeometryContainHoudiniLogo())
							{
								CurrentHoudiniAsset->SetPreviewGeometry(HoudiniMeshTriangles);

								// We need to find corresponding preview component.
								for(TObjectIterator<UHoudiniAssetComponent> It; It; ++It)
								{
									UHoudiniAssetComponent* HoudiniAssetComponent = *It;
									if(HoudiniAssetComponent->HoudiniPreviewAsset && 
											HoudiniAssetComponent->HoudiniPreviewAsset == CurrentHoudiniAsset)
									{
										HoudiniAssetComponent->HoudiniMeshTriangles = HoudiniMeshTriangles;
										HoudiniAssetComponent->UpdateRenderingInformation();
										break;
									}
								}
							}

							// Update properties panel.
							UpdateEditorProperties();
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

					TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
					if(NotificationItem.IsValid())
					{
						NotificationItem->SetText(TaskInfo.StatusText);
						NotificationItem->ExpireAndFadeout();

						NotificationPtr.Reset();
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();

					bStopTicking = true;
					break;
				}

				case EHoudiniEngineTaskState::Aborted:
				case EHoudiniEngineTaskState::FinishedWithErrors:
				{
					HOUDINI_LOG_MESSAGE(TEXT("Failed asset instantiation."));

					TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
					if(NotificationItem.IsValid())
					{
						NotificationItem->SetText(TaskInfo.StatusText);
						NotificationItem->ExpireAndFadeout();

						NotificationPtr.Reset();
					}

					FHoudiniEngine::Get().RemoveTaskInfo(HapiGUID);
					HapiGUID.Invalidate();

					bStopTicking = true;					
					break;
				}

				case EHoudiniEngineTaskState::Processing:
				{
					TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
					if(NotificationItem.IsValid())
					{
						NotificationItem->SetText(TaskInfo.StatusText);
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

		AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(GetOwner());
		check(HoudiniAssetActor);

		// Create new GUID to identify this request.
		HapiGUID = FGuid::NewGuid();

		// Create asset instantiation task object and submit it for processing.
		FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, HapiGUID);
		Task.ActorName = HoudiniAssetActor->GetActorLabel();
		Task.AssetComponent = this;
		FHoudiniEngine::Get().AddTask(Task);

		// We do not want to stop ticking system.
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
	AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(GetOwner());
	if(HoudiniAssetActor)
	{
		GEditor->SelectActor(HoudiniAssetActor, true, true);
	}
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


FPrimitiveSceneProxy*
UHoudiniAssetComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = nullptr;
	
	if(HoudiniMeshTriangles.Num() > 0)
	{
		FScopeLock ScopeLock(&CriticalSectionTriangles);
		Proxy = new FHoudiniMeshSceneProxy(this);
	}
	
	return Proxy;
}


void
UHoudiniAssetComponent::BeginDestroy()
{
	Super::BeginDestroy();
}


void
UHoudiniAssetComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
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
		static const EObjectFlags PatchedClassFlags = RF_Public;

		PatchedClass = ConstructObject<UClass>(UClass::StaticClass(), GetOutermost(), FName(*PatchedClassName), PatchedClassFlags, ClassOfUHoudiniAssetComponent, true);
		PatchedClass->ClassFlags = UHoudiniAssetComponent::StaticClassFlags;
		PatchedClass->ClassCastFlags = UHoudiniAssetComponent::StaticClassCastFlags();
		PatchedClass->ClassConfigName = UHoudiniAssetComponent::StaticConfigName();
		PatchedClass->ClassDefaultObject = GetClass()->ClassDefaultObject;
		PatchedClass->ClassConstructor = ClassOfUHoudiniAssetComponent->ClassConstructor;
		PatchedClass->ClassAddReferencedObjects = ClassOfUHoudiniAssetComponent->ClassAddReferencedObjects;
		PatchedClass->MinAlignment = ClassOfUHoudiniAssetComponent->MinAlignment;
		PatchedClass->PropertiesSize = ClassOfUHoudiniAssetComponent->PropertiesSize;
		PatchedClass->SetSuperStruct(ClassOfUHoudiniAssetComponent->GetSuperStruct());
		PatchedClass->ClassReps = ClassOfUHoudiniAssetComponent->ClassReps;
		PatchedClass->NetFields = ClassOfUHoudiniAssetComponent->NetFields;
		PatchedClass->ReferenceTokenStream = ClassOfUHoudiniAssetComponent->ReferenceTokenStream;
		PatchedClass->NativeFunctionLookupTable = ClassOfUHoudiniAssetComponent->NativeFunctionLookupTable;

		// Patch class information.
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
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
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
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
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
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
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
	Property->SetMetaData(TEXT("Category"), TEXT("HoudiniAsset"));
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
	static const FString CategoryHoudiniAsset = TEXT("HoudiniAsset");
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

	AHoudiniAssetActor* HoudiniAssetActor = CastChecked<AHoudiniAssetActor>(GetOwner());
	if(!HoudiniAssetActor)
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


void
UHoudiniAssetComponent::OnComponentDestroyed()
{
	Super::OnComponentDestroyed();
	HOUDINI_LOG_MESSAGE(TEXT("Destroying component, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);	
}


void
UHoudiniAssetComponent::GetComponentInstanceData(FComponentInstanceDataCache& Cache) const
{
	// Called before we throw away components during RerunConstructionScripts, to cache any data we wish to persist across that operation.
	Super::GetComponentInstanceData(Cache);
	HOUDINI_LOG_MESSAGE(TEXT("Requesting data for caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}


void
UHoudiniAssetComponent::ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache)
{
	// Called after we create new components during RerunConstructionScripts, to optionally apply any data backed up during GetComponentInstanceData.
	Super::ApplyComponentInstanceData(Cache);
	HOUDINI_LOG_MESSAGE(TEXT("Restoring data from caching, Component = 0x%0.8p, HoudiniAsset = 0x%0.8p"), this, HoudiniAsset);
}
