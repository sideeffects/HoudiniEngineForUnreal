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

#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"

#include "EngineUtils.h"
#include "Engine/EngineTypes.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Kismet2/BlueprintEditorUtils.h"	
	#include "SSubobjectBlueprintEditor.h"
#endif

FString
FHoudiniEngineRuntimeUtils::GetLibHAPIName()
{
	static const FString LibHAPIName =

#if PLATFORM_WINDOWS
		HAPI_LIB_OBJECT_WINDOWS;
#elif PLATFORM_MAC
		HAPI_LIB_OBJECT_MAC;
#elif PLATFORM_LINUX
		HAPI_LIB_OBJECT_LINUX;
#else
		TEXT("");
#endif

	return LibHAPIName;
}


void 
FHoudiniEngineRuntimeUtils::GetBoundingBoxesFromActors(const TArray<AActor*> InActors, TArray<FBox>& OutBBoxes)
{
	OutBBoxes.Empty();

	for (auto CurrentActor : InActors)
	{
		if (!IsValid(CurrentActor))
			continue;

		OutBBoxes.Add(CurrentActor->GetComponentsBoundingBox(true, true));
	}
}


bool 
FHoudiniEngineRuntimeUtils::FindActorsOfClassInBounds(UWorld* World, TSubclassOf<AActor> ActorType, const TArray<FBox>& BBoxes, const TArray<AActor*>* ExcludeActors, TArray<AActor*>& OutActors)
{
	if (!IsValid(World))
		return false;
	
	OutActors.Empty();
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* CurrentActor = *ActorItr;
		if (!IsValid(CurrentActor))
			continue;
		
		if (!CurrentActor->GetClass()->IsChildOf(ActorType.Get()))
			continue;

		if (ExcludeActors && ExcludeActors->Contains(CurrentActor))
			continue;

		// Special case
		// Ignore the SkySpheres?
		FString ClassName = CurrentActor->GetClass() ? CurrentActor->GetClass()->GetName() : FString();
		if (ClassName.Contains("BP_Sky_Sphere"))
			continue;

		FBox ActorBounds = CurrentActor->GetComponentsBoundingBox(true);
		for (auto InBounds : BBoxes)
		{
			// Check if both actor's bounds intersects
			if (!ActorBounds.Intersect(InBounds))
				continue;

			OutActors.Add(CurrentActor);
			break;
		}
	}

	return true;
}

bool
FHoudiniEngineRuntimeUtils::SafeDeleteSingleObject(UObject* const InObjectToDelete, UPackage*& OutPackage, bool& bOutPackageIsInMemoryOnly)
{
	bool bDeleted = false;
	OutPackage = nullptr;
	bOutPackageIsInMemoryOnly = false;
	
	if (!IsValid(InObjectToDelete))
		return false;

	// Don't try to delete the object if it has references (we do this here to avoid the FMessageDialog in DeleteSingleObject
	bool bIsReferenced = false;
	bool bIsReferencedByUndo = false;
	if (!GatherObjectReferencersForDeletion(InObjectToDelete, bIsReferenced, bIsReferencedByUndo))
		return false;

	if (bIsReferenced)
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineRuntimeUtils::SafeDeleteSingleObject] Not deleting %s: there are still references to it."), *InObjectToDelete->GetFullName());
	}
	else
	{
		// Even though we already checked for references, we still let DeleteSingleObject check for references, since
		// we want that code path where it'll clean up in-memory references (undo buffer/transactions)
		const bool bCheckForReferences = true;
		if (DeleteSingleObject(InObjectToDelete, bCheckForReferences))
		{
			bDeleted = true;
			
			OutPackage = InObjectToDelete->GetOutermost();
			
			FString PackageFilename;
			if (!IsValid(OutPackage) || !FPackageName::DoesPackageExist(OutPackage->GetName(), &PackageFilename))
			{
				// Package is in memory only, we don't have call CleanUpAfterSuccessfulDelete on it, just do garbage
				// collection to pick up the stale package
				bOutPackageIsInMemoryOnly = true;
			}
			else
			{
				// There is an on-disk package that is now potentially empty, CleanUpAfterSuccessfulDelete must be
				// called on this. Since CleanUpAfterSuccessfulDelete does garbage collection, we return the Package
				// as part of this function so that the caller can collect all Packages and do one call to
				// CleanUpAfterSuccessfulDelete with an array
			}
		}
	}

	return bDeleted;
}

int32
FHoudiniEngineRuntimeUtils::SafeDeleteObjects(TArray<UObject*>& InObjectsToDelete, TArray<UObject*>* OutObjectsNotDeleted)
{
	int32 NumDeleted = 0;
	bool bGarbageCollectionRequired = false;
	TSet<UPackage*> PackagesToCleanUp;
	TSet<UObject*> ProcessedObjects;
	while (InObjectsToDelete.Num() > 0)
	{
		UObject* const ObjectToDelete = InObjectsToDelete.Pop();

		if (ProcessedObjects.Contains(ObjectToDelete))
			continue;
		
		ProcessedObjects.Add(ObjectToDelete);
		
		if (!IsValid(ObjectToDelete))
			continue;

		UPackage* Package = nullptr;
		bool bInMemoryPackageOnly = false;
		if (SafeDeleteSingleObject(ObjectToDelete, Package, bInMemoryPackageOnly))
		{
			NumDeleted++;
			if (bInMemoryPackageOnly)
			{
				// Packages that are in-memory only are cleaned up by garbage collection
				if (!bGarbageCollectionRequired)
					bGarbageCollectionRequired = true;
			}
			else
			{
				// Clean up potentially empty packages in one call to CleanupAfterSuccessfulDelete at the end
				PackagesToCleanUp.Add(Package);
			}
		}
		else if (OutObjectsNotDeleted)
		{
			OutObjectsNotDeleted->Add(ObjectToDelete);
		}
	}

	// CleanupAfterSuccessfulDelete calls CollectGarbage, so don't call it here if we have PackagesToCleanUp
	if (bGarbageCollectionRequired && PackagesToCleanUp.Num() <= 0)
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	if (PackagesToCleanUp.Num() > 0)
		CleanupAfterSuccessfulDelete(PackagesToCleanUp.Array());

	return NumDeleted;
}


#if WITH_EDITOR
int32
FHoudiniEngineRuntimeUtils::CopyComponentProperties(UActorComponent* SourceComponent, UActorComponent* TargetComponent, const EditorUtilities::FCopyOptions& Options)
{
	UClass* ComponentClass = SourceComponent->GetClass();
	check( ComponentClass == TargetComponent->GetClass() );

	const bool bIsPreviewing = ( Options.Flags & EditorUtilities::ECopyOptions::PreviewOnly ) != 0;
	int32 CopiedPropertyCount = 0;
	bool bTransformChanged = false;

	// Build a list of matching component archetype instances for propagation (if requested)
	TArray<UActorComponent*> ComponentArchetypeInstances;
	if( Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances )
	{
		TArray<UObject*> Instances;
		TargetComponent->GetArchetypeInstances(Instances);
		for(UObject* ObjInstance : Instances)
		{
			UActorComponent* ComponentInstance = Cast<UActorComponent>(ObjInstance);
			if (ComponentInstance && ComponentInstance != SourceComponent && ComponentInstance != TargetComponent)
			{ 
				ComponentArchetypeInstances.Add(ComponentInstance);
			}
		}
	}

	TSet<const FProperty*> SourceUCSModifiedProperties;
	SourceComponent->GetUCSModifiedProperties(SourceUCSModifiedProperties);

	TArray<UActorComponent*> ComponentInstancesToReregister;

	// Copy component properties
	for( FProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsIdentical = Property->Identical_InContainer( SourceComponent, TargetComponent );
		const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
		const bool bIsTransform =
			Property->GetFName() == USceneComponent::GetRelativeScale3DPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeLocationPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeRotationPropertyName();

		// auto SourceComponentIsRoot = [&]()
		// {
		// 	USceneComponent* RootComponent = SourceActor->GetRootComponent();
		// 	if (SourceComponent == RootComponent)
		// 	{
		// 		return true;
		// 	}
		// 	else if (RootComponent == nullptr && bSourceActorIsBPCDO)
		// 	{
		// 		// If we're dealing with a BP CDO as source, then look at the target for whether this is the root component
		// 		return (TargetComponent == TargetActor->GetRootComponent());
		// 	}
		// 	return false;
		// };

		TSet<UObject*> ModifiedObjects;
		// if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
		// 	&& ( !bIsTransform || (!bSourceActorIsCDO && !bTargetActorIsCDO) || !SourceComponentIsRoot() ) )
		if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
			&& ( !bIsTransform ))
		{
			const bool bIsSafeToCopy = (!(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)))
				                    && (!(Options.Flags & EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties) || (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate)));
			if( bIsSafeToCopy )
			{
				// if (!Options.CanCopyProperty(*Property, *SourceActor))
				// {
				// 	continue;
				// }
				if (!Options.CanCopyProperty(*Property, *SourceComponent))
				{
					continue;
				}
					
				if( !bIsPreviewing )
				{
					if( !ModifiedObjects.Contains(TargetComponent) )
					{
						TargetComponent->SetFlags(RF_Transactional);
						TargetComponent->Modify();
						ModifiedObjects.Add(TargetComponent);
					}

					if( Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty )
					{
						TargetComponent->PreEditChange( Property );
					}

					// Determine which component archetype instances match the current property value of the target component (before it gets changed). We only want to propagate the change to those instances.
					TArray<UActorComponent*> ComponentArchetypeInstancesToChange;
					if( Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances )
					{
						for (UActorComponent* ComponentArchetypeInstance : ComponentArchetypeInstances)
						{
							if( ComponentArchetypeInstance != nullptr && Property->Identical_InContainer( ComponentArchetypeInstance, TargetComponent ) )
							{
								bool bAdd = true;
								// We also need to double check that either the direct archetype of the target is also identical
								if (ComponentArchetypeInstance->GetArchetype() != TargetComponent)
								{
									UActorComponent* CheckComponent = CastChecked<UActorComponent>(ComponentArchetypeInstance->GetArchetype());
									while (CheckComponent != ComponentArchetypeInstance)
									{
										if (!Property->Identical_InContainer( CheckComponent, TargetComponent ))
										{
											bAdd = false;
											break;
										}
										CheckComponent = CastChecked<UActorComponent>(CheckComponent->GetArchetype());
									}
								}
									
								if (bAdd)
								{
									ComponentArchetypeInstancesToChange.Add( ComponentArchetypeInstance );
								}
							}
						}
					}

					EditorUtilities::CopySingleProperty(SourceComponent, TargetComponent, Property);

					if( Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty )
					{
						FPropertyChangedEvent PropertyChangedEvent( Property );
						TargetComponent->PostEditChangeProperty( PropertyChangedEvent );
					}

					if( Options.Flags & EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances )
					{
						for( int32 InstanceIndex = 0; InstanceIndex < ComponentArchetypeInstancesToChange.Num(); ++InstanceIndex )
						{
							UActorComponent* ComponentArchetypeInstance = ComponentArchetypeInstancesToChange[InstanceIndex];
							if( ComponentArchetypeInstance != nullptr )
							{
								if( !ModifiedObjects.Contains(ComponentArchetypeInstance) )
								{
									// Ensure that this instance will be included in any undo/redo operations, and record it into the transaction buffer.
									// Note: We don't do this for components that originate from script, because they will be re-instanced from the template after an undo, so there is no need to record them.
									if (!ComponentArchetypeInstance->IsCreatedByConstructionScript())
									{
										ComponentArchetypeInstance->SetFlags(RF_Transactional);
										ComponentArchetypeInstance->Modify();
										ModifiedObjects.Add(ComponentArchetypeInstance);
									}

									// We must also modify the owner, because we'll need script components to be reconstructed as part of an undo operation.
									AActor* Owner = ComponentArchetypeInstance->GetOwner();
									if( Owner != nullptr && !ModifiedObjects.Contains(Owner))
									{
										Owner->Modify();
										ModifiedObjects.Add(Owner);
									}
								}

								if (ComponentArchetypeInstance->IsRegistered())
								{
									ComponentArchetypeInstance->UnregisterComponent();
									ComponentInstancesToReregister.Add(ComponentArchetypeInstance);
								}

								EditorUtilities::CopySingleProperty( TargetComponent, ComponentArchetypeInstance, Property );
							}
						}
					}
				}

				++CopiedPropertyCount;

				if( bIsTransform )
				{
					bTransformChanged = true;
				}
			}
		}
	}

	for (UActorComponent* ModifiedComponentInstance : ComponentInstancesToReregister)
	{
		ModifiedComponentInstance->RegisterComponent();
	}

	return CopiedPropertyCount;
}
#endif


#if WITH_EDITOR
FBlueprintEditor*
FHoudiniEngineRuntimeUtils::GetBlueprintEditor(const UObject* InObject)
{
	if (!IsValid(InObject))
		return nullptr;

	UObject* Outer = InObject->GetOuter();
	if (!IsValid(Outer))
		return nullptr;

	UBlueprintGeneratedClass* OuterBPClass = Cast<UBlueprintGeneratedClass>(Outer->GetClass());

	if (!OuterBPClass)
		return nullptr;

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return static_cast<FBlueprintEditor*>(AssetEditorSubsystem->FindEditorForAsset(OuterBPClass->ClassGeneratedBy, false));
}
#endif


#if WITH_EDITOR
void 
FHoudiniEngineRuntimeUtils::MarkBlueprintAsStructurallyModified(UActorComponent* ComponentTemplate)
{
	if (!ComponentTemplate)
		return;

	UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ComponentTemplate->GetOuter());
	if (!BPGC)
		return;

	UBlueprint* Blueprint = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
	if (!Blueprint)
		return;

	Blueprint->Modify();

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(AssetEditorSubsystem->FindEditorForAsset(Blueprint, false));
	check(BlueprintEditor);

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (SCS)
	{
		SCS->SaveToTransactionBuffer();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	BlueprintEditor->GetSubobjectEditor()->UpdateTree(true);
}
#endif

#if WITH_EDITOR
void 
FHoudiniEngineRuntimeUtils::MarkBlueprintAsModified(UActorComponent* ComponentTemplate)
{
	if (!ComponentTemplate)
		return;

	UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ComponentTemplate->GetOuter());
	if (!BPGC)
		return;

	UBlueprint* Blueprint = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
	if (!Blueprint)
		return;

	Blueprint->Modify();

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}
#endif

FTransform FHoudiniEngineRuntimeUtils::CalculateHoudiniLandscapeTransform(ULandscapeInfo* LandscapeInfo)
{
#if WITH_EDITOR
	ALandscapeProxy* LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
	FTransform OutTransform = LandscapeProxy->GetTransform();
		
	FVector LandscapeScale = OutTransform.GetScale3D();

	// The final landscape transform that should go into Houdini consist of the following two components:
	// - Shared Landscape Transform
	// - Extents of all the loaded landscape components

	// The houdini transform will always be in the center of the currently loaded landscape components.

	FIntRect Extent;
	Extent.Min.X = INT32_MAX;
	Extent.Min.Y = INT32_MAX;
	Extent.Max.X = INT32_MIN;
	Extent.Max.Y = INT32_MIN;
	
	LandscapeInfo->ForAllLandscapeComponents([&Extent](ULandscapeComponent* LandscapeComponent)
	{
		LandscapeComponent->GetComponentExtent(Extent.Min.X, Extent.Min.Y, Extent.Max.X, Extent.Max.Y);
	});

	FIntVector ExtentCenter(
			(Extent.Min.X + Extent.Max.X)/2,
			(Extent.Min.Y + Extent.Max.Y)/2,
			1);
	
	FVector ExtentMin = FVector(Extent.Min.X * LandscapeScale.X, Extent.Min.Y * LandscapeScale.Y, 1.0);
	FVector ExtentMax = FVector(Extent.Max.X * LandscapeScale.X, Extent.Max.Y * LandscapeScale.Y, 1.0);
	
	// Add section base offset to the landscape transform
	FVector Loc = OutTransform.GetLocation();
	Loc.X += ExtentCenter.X * LandscapeScale.X;
	Loc.Y += ExtentCenter.Y * LandscapeScale.Y;
	
	OutTransform.SetLocation(Loc);

	return OutTransform;
#else
	return FTransform();
#endif
}


#if WITH_EDITOR

// Centralized call to set actor label (changing Actor's implementation was too risky)
bool FHoudiniEngineRuntimeUtils::SetActorLabel(AActor* Actor, const FString& ActorLabel)
{
	// Clean up the incoming string a bit
	FString NewActorLabel = ActorLabel.TrimStartAndEnd();
	if (NewActorLabel == Actor->GetActorLabel(/*bCreateIfNone*/false))
	{
		return false;
	}
	Actor->SetActorLabel(NewActorLabel);
	return true;
}

void
FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(UObject* Obj, FName PropertyName)
{
	FPropertyChangedEvent Evt(FindFieldChecked<FProperty>(Obj->GetClass(), PropertyName));
	Obj->PostEditChangeProperty(Evt);
}

void FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(UObject* Obj, FProperty* Property)
{
	FPropertyChangedEvent Evt(Property);
	Obj->PostEditChangeProperty(Evt);
}

void FHoudiniEngineRuntimeUtils::PropagateObjectDeltaChangeToArchetypeInstance(UObject* InObject, const FTransactionObjectDeltaChange& DeltaChange)
{
	if (!InObject)
		return;
	if (!InObject->HasAnyFlags(RF_ArchetypeObject))
		return;

	// Iterate over the modified properties and propagate value changed to all archetype instances
	TArray<UObject*> ArchetypeInstances;
	InObject->GetArchetypeInstances(ArchetypeInstances);
	for (UObject* Instance : ArchetypeInstances)
	{
		UE_LOG(LogTemp, Log, TEXT("[void FHoudiniEngineRuntimeUtils::PropagateTransactionToArchetypeInstance] Found Archetype instance: %s"), *(Instance->GetPathName()));
		for (FName PropertyName : DeltaChange.ChangedProperties)
		{
			UE_LOG(LogTemp, Log, TEXT("[void FHoudiniEngineRuntimeUtils::PropagateTransactionToArchetypeInstance] Changed property: %s"), *(PropertyName.ToString()));
			// FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->GetRelativeLocation_DirectMutable(), OldRelativeLocation, SelectedTemplate->GetRelativeLocation());
		}
	}
}

void FHoudiniEngineRuntimeUtils::ForAllArchetypeInstances(UObject* InTemplateObj, TFunctionRef<void(UObject* Obj)> Operation)
{
	if (!InTemplateObj)
		return;
	if (!InTemplateObj->HasAnyFlags(RF_ArchetypeObject|RF_DefaultSubObject))
		return;
	
	TArray<UObject*> Instances; 
	InTemplateObj->GetArchetypeInstances(Instances);
	
	for(UObject* Instance : Instances)
	{
		Operation(Instance);
	}
}
#endif

FHoudiniStaticMeshGenerationProperties
FHoudiniEngineRuntimeUtils::GetDefaultStaticMeshGenerationProperties()
{
	FHoudiniStaticMeshGenerationProperties SMGP;

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
	{
		SMGP.bGeneratedDoubleSidedGeometry = HoudiniRuntimeSettings->bDoubleSidedGeometry;
		SMGP.GeneratedPhysMaterial = HoudiniRuntimeSettings->PhysMaterial;
		SMGP.DefaultBodyInstance = HoudiniRuntimeSettings->DefaultBodyInstance;
		SMGP.GeneratedCollisionTraceFlag = HoudiniRuntimeSettings->CollisionTraceFlag;
		//SMGP.GeneratedLpvBiasMultiplier = HoudiniRuntimeSettings->LpvBiasMultiplier;
		SMGP.GeneratedLightMapResolution = HoudiniRuntimeSettings->LightMapResolution;
		SMGP.GeneratedLightMapCoordinateIndex = HoudiniRuntimeSettings->LightMapCoordinateIndex;
		SMGP.bGeneratedUseMaximumStreamingTexelRatio = HoudiniRuntimeSettings->bUseMaximumStreamingTexelRatio;
		SMGP.GeneratedStreamingDistanceMultiplier = HoudiniRuntimeSettings->StreamingDistanceMultiplier;
		SMGP.GeneratedWalkableSlopeOverride = HoudiniRuntimeSettings->WalkableSlopeOverride;
		SMGP.GeneratedFoliageDefaultSettings = HoudiniRuntimeSettings->FoliageDefaultSettings;
		SMGP.GeneratedAssetUserData = HoudiniRuntimeSettings->AssetUserData;
	}

	return SMGP;
}

FMeshBuildSettings
FHoudiniEngineRuntimeUtils::GetDefaultMeshBuildSettings()
{
	FMeshBuildSettings DefaultBuildSettings;

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if(HoudiniRuntimeSettings)
	{
		DefaultBuildSettings.bRemoveDegenerates = HoudiniRuntimeSettings->bRemoveDegenerates;
		DefaultBuildSettings.bUseMikkTSpace = HoudiniRuntimeSettings->bUseMikkTSpace;
		//DefaultBuildSettings.bBuildAdjacencyBuffer = HoudiniRuntimeSettings->bBuildAdjacencyBuffer;
		DefaultBuildSettings.MinLightmapResolution = HoudiniRuntimeSettings->MinLightmapResolution;
		DefaultBuildSettings.bUseFullPrecisionUVs = HoudiniRuntimeSettings->bUseFullPrecisionUVs;
		DefaultBuildSettings.SrcLightmapIndex = HoudiniRuntimeSettings->SrcLightmapIndex;
		DefaultBuildSettings.DstLightmapIndex = HoudiniRuntimeSettings->DstLightmapIndex;

		DefaultBuildSettings.bComputeWeightedNormals = HoudiniRuntimeSettings->bComputeWeightedNormals;
		DefaultBuildSettings.bBuildReversedIndexBuffer = HoudiniRuntimeSettings->bBuildReversedIndexBuffer;
		DefaultBuildSettings.bUseHighPrecisionTangentBasis = HoudiniRuntimeSettings->bUseHighPrecisionTangentBasis;
		DefaultBuildSettings.bGenerateDistanceFieldAsIfTwoSided = HoudiniRuntimeSettings->bGenerateDistanceFieldAsIfTwoSided;
		DefaultBuildSettings.bSupportFaceRemap = HoudiniRuntimeSettings->bSupportFaceRemap;
		DefaultBuildSettings.DistanceFieldResolutionScale = HoudiniRuntimeSettings->DistanceFieldResolutionScale;

		// Recomputing normals.
		EHoudiniRuntimeSettingsRecomputeFlag RecomputeNormalFlag = (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeNormalsFlag;
		switch (RecomputeNormalFlag)
		{
			case HRSRF_Never:
			{
				DefaultBuildSettings.bRecomputeNormals = false;
				break;
			}

			case HRSRF_Always:
			case HRSRF_OnlyIfMissing:
			default:
			{
				DefaultBuildSettings.bRecomputeNormals = true;
				break;
			}
		}

		// Recomputing tangents.
		EHoudiniRuntimeSettingsRecomputeFlag RecomputeTangentFlag = (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeTangentsFlag;
		switch (RecomputeTangentFlag)
		{
			case HRSRF_Never:
			{
				DefaultBuildSettings.bRecomputeTangents = false;
				break;
			}

			case HRSRF_Always:
			case HRSRF_OnlyIfMissing:
			default:
			{
				DefaultBuildSettings.bRecomputeTangents = true;
				break;
			}
		}

		// Lightmap UV generation.
		EHoudiniRuntimeSettingsRecomputeFlag GenerateLightmapUVFlag = (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeTangentsFlag;
		switch (GenerateLightmapUVFlag)
		{
			case HRSRF_Never:
			{
				DefaultBuildSettings.bGenerateLightmapUVs = false;
				break;
			}

			case HRSRF_Always:
			case HRSRF_OnlyIfMissing:
			default:
			{
				DefaultBuildSettings.bGenerateLightmapUVs = true;
				break;
			}
		}
	}

	return DefaultBuildSettings;
}

