#include "HoudiniMeshTranslatorUtils.h"

#include "HoudiniMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniCookable.h"
#include "HoudiniEngine.h"
#include "HoudiniOutput.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniMaterialTranslator.h"
#include "HoudiniAssetActor.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniStaticMeshComponent.h"
#include "HoudiniSkeletalMeshTranslator.h"

#include "Engine/StaticMeshSocket.h"

#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "PackageTools.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "MeshDescriptionOperations.h"

#include "BSPOps.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "GeometryToolsEngine.h"
#include "Engine/SkeletalMesh.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniEngineString.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/DynamicMeshComponent.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "EditorSupportDelegates.h"
#include "HoudiniGeometryCollectionTranslator.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if WITH_EDITOR
	#include "ConvexDecompTool.h"
	#include "Editor/UnrealEd/Private/GeomFitUtils.h"
	#include "LevelEditorViewport.h"
	#include "FileHelpers.h"
#endif

#include <HoudiniHLODLayerUtils.h>

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

bool
FHoudiniMeshTranslatorUtils::IsMeshBuildTimingEnabled()
{
	return CVarHoudiniEngineMeshBuildTimer.GetValueOnAnyThread() != 0.0;
}

void
FHoudiniMeshTranslatorUtils::UpdateMeshComponent(
	UMeshComponent *InMeshComponent, 
	UObject* InMesh, 
	const FHoudiniOutputObjectIdentifier &InOutputIdentifier,
	const FHoudiniOutputObject& OutputObject,
	const FHoudiniGeoPartObject *InHGPO, 
	TArray<TObjectPtr<AActor>> &HoudiniCreatedSocketActors,
	TArray<TObjectPtr<AActor>> &HoudiniAttachedSocketActors,
	bool bInApplyGenericProperties)
{
	UStaticMeshComponent* const SMC = Cast<UStaticMeshComponent>(InMeshComponent);
	UHoudiniStaticMeshComponent* const HSMC = Cast<UHoudiniStaticMeshComponent>(InMeshComponent);
	if (IsValid(SMC))
	{
		FHoudiniStaticMeshTranslator::UpdateMeshOnStaticMeshComponent(SMC, InMesh);
	}
	else if (IsValid(HSMC))
	{
		FHoudiniProxyMeshTranslator::UpdateMeshOnHoudiniStaticMeshComponent(HSMC, InMesh);
	}
	
	// Update collision/visibility
	EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(InOutputIdentifier.SplitIdentifier);
	if (SplitType == EHoudiniSplitType::InvisibleComplexCollider || OutputObject.bIsInvisibleCollisionMesh)
	{
		// Invisible complex collider should not be seen
		InMeshComponent->SetVisibility(false);
		InMeshComponent->SetHiddenInGame(true);
		InMeshComponent->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
		InMeshComponent->SetCastShadow(false);
	}
	else
	{
		// Instanced mesh parts are hidden by HAPI because their packed output supplies visibility.
		// Dynamic Mesh components represent that packed output directly, so they are visible unless
		// the output object is an invisible collision mesh (handled above).
		bool bVisible = true;
		if (InHGPO && !InHGPO->bIsInstanced)
			bVisible = InHGPO->bIsVisible;
		InMeshComponent->SetVisibility(bVisible);
		InMeshComponent->SetHiddenInGame(!bVisible);
		
		FPropertyChangedEvent Evt(FindFieldChecked<FProperty>(InMeshComponent->GetClass(), "bVisible"));
		InMeshComponent->PostEditChangeProperty(Evt);
	}

	// TODO:
	// Update navmesh?

	// Transform the component by transformation provided by HAPI.
	InMeshComponent->SetRelativeTransform(InHGPO ? InHGPO->TransformMatrix : FTransform::Identity);

	// If the static mesh had sockets, we can assign the desired actor to them now
	UStaticMeshComponent * StaticMeshComponent = Cast<UStaticMeshComponent>(InMeshComponent);
	UStaticMesh * StaticMesh = nullptr;
	if (IsValid(StaticMeshComponent))
		StaticMesh = StaticMeshComponent->GetStaticMesh();

	if (StaticMesh && IsValid(StaticMesh)) 
	{
		int32 NumberOfSockets = StaticMesh == nullptr ? 0 : StaticMesh->Sockets.Num();
		for (int32 nSocket = 0; nSocket < NumberOfSockets; nSocket++)
		{
			UStaticMeshSocket* MeshSocket = StaticMesh->Sockets[nSocket];
			if (IsValid(MeshSocket) && (MeshSocket->Tag.IsEmpty()))
				continue;

			FHoudiniStaticMeshTranslator::AddActorsToMeshSocket(StaticMesh->Sockets[nSocket], StaticMeshComponent, HoudiniCreatedSocketActors, HoudiniAttachedSocketActors);
		}

		// Iterate all remaining created socket actors, destroy the ones that are not assigned to socket after re-cook
		{
			for (int32 Idx = HoudiniCreatedSocketActors.Num() - 1; Idx >= 0; --Idx) 
			{
				AActor * CurActor = HoudiniCreatedSocketActors[Idx];

				if (!IsValid(CurActor))
				{
					HoudiniCreatedSocketActors.RemoveAt(Idx);
					continue;
				}

				bool bFoundSocket = false;
				for (auto & CurSocket : StaticMesh->Sockets)
				{
					if (CurSocket->SocketName == CurActor->GetAttachParentSocketName())
					{
						bFoundSocket = true;
						break;
					}
				}
				// cur actor's attaching socket is found, skip
				if (bFoundSocket)
					continue;

				// Destroy the previous created socket actor if not found
				HoudiniCreatedSocketActors.RemoveAt(Idx);
				CurActor->Destroy();
			}
		}

		// Detach the in level actors which is not attached to any socket now
		{
			for (int32 Idx = HoudiniAttachedSocketActors.Num() - 1; Idx >= 0; --Idx) 
			{
				AActor* CurActor = HoudiniAttachedSocketActors[Idx];
				if (!IsValid(CurActor)) 
				{
					HoudiniAttachedSocketActors.RemoveAt(Idx);
					continue;
				}

				bool bFoundSocket = false;
				for (auto & CurSocket : StaticMesh->Sockets)
				{
					if (CurSocket->SocketName == CurActor->GetAttachParentSocketName())
					{
						bFoundSocket = true;
						break;
					}
				}

				if (bFoundSocket)
					continue;

				// If the attached socket name is not found in current socket, detach it and remove from the array
				CurActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
				HoudiniAttachedSocketActors.RemoveAt(Idx);			
			}
		}

	}

	if (bInApplyGenericProperties)
	{
		// Clear the component tags, if permitted by HGPOs
		FHoudiniEngineUtils::KeepOrClearComponentTags(InMeshComponent, InHGPO);
		
		// Update the property attributes on the component
		TArray<FHoudiniGenericAttribute> PropertyAttributes;
		if (FHoudiniEngineUtils::GetGenericPropertiesAttributes(
			InOutputIdentifier.GeoId, InOutputIdentifier.PartId,
			true,
			InOutputIdentifier.PrimitiveIndex,
			INDEX_NONE,
			InOutputIdentifier.PointIndex,
			PropertyAttributes))
		{
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(InMeshComponent, PropertyAttributes);
		}
	}

	// Custom Primitive Data
	{
		TArray<float> CustomPrimitiveData;
		FHoudiniMeshTranslatorUtils::GetCustomPrimitiveData(
			InOutputIdentifier.GeoId, InOutputIdentifier.PartId, InOutputIdentifier.PrimitiveIndex, CustomPrimitiveData);

		FHoudiniMeshTranslatorUtils::SetCustomPrimitiveData(CustomPrimitiveData, InMeshComponent);
	}
}

void
FHoudiniMeshTranslatorUtils::UpdateStaticMeshNaniteSettings(int32 GeoId, int32 PartId, int32 PrimIndex, UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
		return;

	// 
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	// Start by looking for the nanite enabled attribute, disabled by default
	bool bEnableNanite = false;
	TArray<int32> IntData;

	FHoudiniHapiAccessor Accessor;
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_ENABLED);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, IntData, PrimIndex, 1))
	{
		Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, IntData, 0, 1);
	}
	if (IntData.Num() > 0)
	{
		bEnableNanite = (IntData[0] != 0);
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	FMeshNaniteSettings NaniteSettings = StaticMesh->GetNaniteSettings();
#endif

	// Then look for the position precision attribute, auto by default (MIN_int32)
	IntData.Empty();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	NaniteSettings.PositionPrecision = MIN_int32;
#else
	StaticMesh->NaniteSettings.PositionPrecision = MIN_int32;
#endif
	// Look for a specific prim attribute first
	Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_POSITION_PRECISION);
	if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, IntData, PrimIndex, 1))
	{
		Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, IntData, 0, 1);
	}

	if (IntData.Num() > 0)
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		NaniteSettings.PositionPrecision = IntData[0];
#else
		StaticMesh->NaniteSettings.PositionPrecision = IntData[0];
#endif
	}

	// Look for the percent triangle attribute, one by default (all triangles)
	// as this mesh is also used in the physics engine as the complex collision version

	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		NaniteSettings.FallbackPercentTriangles = 1.0f;
#else
		StaticMesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
#endif
		TArray<float> FloatData;
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_PERCENT_TRIANGLES);

		// Look for a specific prim attribute first
		if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, 1, FloatData, PrimIndex, 1))
		{
			//Global search for the attribute
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, FloatData, PrimIndex, 1);
		}

		if (FloatData.Num() > 0)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			// If a nanite percent triangles attribute was found, we likely also want to set the fallback target to PercentTriangles
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			NaniteSettings.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
#else
			StaticMesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
#endif
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			NaniteSettings.FallbackPercentTriangles = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
#else
			StaticMesh->NaniteSettings.FallbackPercentTriangles = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
#endif
		}
	}

	{
		// Also look for an attribute setting the relative error (default to 1)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		NaniteSettings.FallbackRelativeError = 1.0f;
#else
		StaticMesh->NaniteSettings.FallbackRelativeError = 1.0f;
#endif

		TArray<float> FloatData;
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_FB_RELATIVE_ERROR);
		if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, FloatData, PrimIndex, 1))
		{
			//Global search for the attribute
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, FloatData, PrimIndex, 1);
		}

		if (FloatData.Num() > 0)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			// If a nanite relative error attribute was found, we likely also want to set the fallback target to RelativeError
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			NaniteSettings.FallbackTarget = ENaniteFallbackTarget::RelativeError;
#else
			StaticMesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::RelativeError;
#endif
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			NaniteSettings.FallbackRelativeError = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
#else
			StaticMesh->NaniteSettings.FallbackRelativeError = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
#endif
		}
	}

	{
		// And do the same for the trim relative error (default to 0)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
		NaniteSettings.TrimRelativeError = 0.0f;
#else
		StaticMesh->NaniteSettings.TrimRelativeError = 0.0f;
#endif
		TArray<float> FloatData;
		Accessor.Init(GeoId, PartId, HAPI_UNREAL_ATTRIB_NANITE_TRIM_RELATIVE_ERROR);
		if (!Accessor.GetAttributeData(HAPI_ATTROWNER_PRIM, FloatData, PrimIndex, 1))
		{
			//Global search for the attribute
			Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, 1, FloatData, PrimIndex, 1);
		}

		if (FloatData.Num() > 0)
		{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			NaniteSettings.TrimRelativeError = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
#else
			StaticMesh->NaniteSettings.TrimRelativeError = FMath::Clamp<float>(FloatData[0], 0.0f, 1.0f);
#endif
		}
	}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
	NaniteSettings.bEnabled = bEnableNanite;
	StaticMesh->SetNaniteSettings(NaniteSettings);
#else
	StaticMesh->NaniteSettings.bEnabled = bEnableNanite;
#endif
}

void FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(
	const FHoudiniGeoPartObject& InHGPO, int32 InPointIndex, int32 InPrimIndex, TMap<FString, FString>& OutAttributes, TMap<FString, FString>& OutTokens)
{
	// Get all the supported attributes from the HGPO
	{
		FString TempFolder;
		if (FHoudiniEngineUtils::GetTempFolderAttribute(InHGPO.GeoId, TempFolder, InHGPO.PartId, InPrimIndex))
		{
			if (!TempFolder.IsEmpty())
			{
				// cache the level path attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_TEMP_FOLDER, TempFolder);
			}
		}
	}

	{
		FString LevelPath;
		if (FHoudiniEngineUtils::GetLevelPathAttribute(InHGPO.GeoId, InHGPO.PartId, LevelPath, InPointIndex, InPrimIndex))
		{
			if (!LevelPath.IsEmpty())
			{
				// cache the level path attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_LEVEL_PATH, LevelPath);
			}
		}
	}

	{
		FString OutputName;
		if (FHoudiniEngineUtils::GetOutputNameAttribute(InHGPO.GeoId, InHGPO.PartId, OutputName, InPointIndex, InPrimIndex))
		{
			if (!OutputName.IsEmpty())
			{
				// cache the output name attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2, OutputName);
			}
		}
	}

	{
		FString BakeName;
		if (FHoudiniEngineUtils::GetBakeNameAttribute(InHGPO.GeoId, InHGPO.PartId, BakeName, InPointIndex, InPrimIndex))
		{
			if (!BakeName.IsEmpty())
			{
				// cache the bake name attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_NAME, BakeName);
			}
		}
	}

	{
		int32 TileValue;
		if (FHoudiniEngineUtils::GetTileAttribute(InHGPO.GeoId, InHGPO.PartId, TileValue, InPointIndex, InPrimIndex))
		{
			if (TileValue >= 0)
			{
				// cache the tile attribute as a token on the output object
				OutTokens.Add(TEXT("tile"), FString::FromInt(TileValue));
			}
		}
	}

	{
		FString BakeOutputActorName;
		if (FHoudiniEngineUtils::GetBakeActorAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutputActorName, InPointIndex, InPrimIndex))
		{
			if (!BakeOutputActorName.IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR, BakeOutputActorName);
			}
		}
	}

	{
		FString BakeOutputActorClassName;
		if (FHoudiniEngineUtils::GetBakeActorClassAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutputActorClassName, InPointIndex, InPrimIndex))
		{
			if (!BakeOutputActorClassName.IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_ACTOR_CLASS, BakeOutputActorClassName);
			}
		}
	}

	{
		FString BakeFolder;
		if (FHoudiniEngineUtils::GetBakeFolderAttribute(InHGPO.GeoId, InHGPO.PartId, BakeFolder, InPrimIndex))
		{
			if (!BakeFolder.IsEmpty())
			{
				// cache the unreal_bake_folder attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_FOLDER, BakeFolder);
			}
		}
	}

	{
		FString BakeOutlinerFolder;
		if (FHoudiniEngineUtils::GetBakeOutlinerFolderAttribute(InHGPO.GeoId, InHGPO.PartId, BakeOutlinerFolder, InPointIndex, InPrimIndex))
		{
			if (!BakeOutlinerFolder.IsEmpty())
			{
				// cache the bake actor attribute on the output object
				OutAttributes.Add(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER, BakeOutlinerFolder);
			}
		}
	}
}

void FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniCachedMeshPart& InCachedMeshPart,
	const FString& InSplitGroupName,
	TMap<FString, FString>& OutAttributes,
	TMap<FString, FString>& OutTokens)
{
	const FPreparedHoudiniMeshSplit& PreparedSplit = InCachedMeshPart.Splits.FindChecked(InSplitGroupName);
	int32 PointIndex = INDEX_NONE;
	const int32 PrimIndex = PreparedSplit.FirstValidPrimIndex;

	const int32 FirstValidVertexIndex = PreparedSplit.FirstValidVertexIndex;
	if (FirstValidVertexIndex >= 0 && PreparedSplit.VertexList.IsValidIndex(FirstValidVertexIndex))
	{
		PointIndex = PreparedSplit.VertexList[FirstValidVertexIndex];
	}

	CopyAttributesFromHGPOForSplit(InHGPO, PointIndex, PrimIndex, OutAttributes, OutTokens);
}

void FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(
	const FHoudiniGeoPartObject& InHGPO,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	TMap<FString, FString>& OutAttributes,
	TMap<FString, FString>& OutTokens)
{
	const int32 PrimIndex = InOutputObjectIdentifier.PrimitiveIndex;
	const int32 PointIndex = InOutputObjectIdentifier.PointIndex;

	CopyAttributesFromHGPOForSplit(InHGPO, PointIndex, PrimIndex, OutAttributes, OutTokens);
}

FHoudiniOutputObjectIdentifier
FHoudiniMeshTranslatorUtils::MakeOutputObjectIdentifier(
	const FHoudiniGeoPartObject& HGPO,
	const FHoudiniCachedMeshPart& InCachedMeshPart,
	const FString& InSplitGroupName,
	EHoudiniSplitType InSplitType)
{
	// Helper to make and populate a FHoudiniOutputObjectIdentifier from the current HGPO and the given
// InSplitGroupName and InSplitType.

	const FPreparedHoudiniMeshSplit& PreparedSplit = InCachedMeshPart.Splits.FindChecked(InSplitGroupName);
	FHoudiniOutputObjectIdentifier OutputObjectIdentifier(
		HGPO.ObjectId, HGPO.GeoId, HGPO.PartId, GetMeshIdentifierFromSplit(InSplitGroupName, InSplitType));
	OutputObjectIdentifier.PartName = HGPO.PartName;
	OutputObjectIdentifier.PrimitiveIndex = PreparedSplit.FirstValidPrimIndex;
	const int32 FirstValidVertexIndex = PreparedSplit.FirstValidVertexIndex;
	if (FirstValidVertexIndex >= 0 && PreparedSplit.VertexList.IsValidIndex(FirstValidVertexIndex))
	{
		OutputObjectIdentifier.PointIndex = PreparedSplit.VertexList[FirstValidVertexIndex];
	}
	else
	{
		OutputObjectIdentifier.PointIndex = -1;
	}

	return OutputObjectIdentifier;
}

bool
FHoudiniMeshTranslatorUtils::CreateNeededMaterials(
	const FHoudiniGeoPartObject& HGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	FHoudiniPackageParams& InOutPackageParams,
	UObject* InOuterComponent,
	const FHoudiniMeshTranslationContext& InTranslationContext,
	TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InOutOutputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InInputAssignmentMaterials,
	const TMap<FHoudiniMaterialIdentifier, TObjectPtr<UMaterialInterface>>& InAllOutputMaterials)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslatorUtils::CreateNeededMaterials);
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;

	InOutCachedMeshPart.GetUniqueMaterialIds(HGPO);

	// Update package params with resolved attributes
	TMap<FString, FString> Attributes;
	TMap<FString, FString> Tokens;
	FHoudiniAttributeResolver Resolver;
	FHoudiniPackageParams FinalPackageParams;

	// Get the attributes from normal geo, or first LOD if there is no normal geo. Fallback to use index 0.
	FString SplitToUse;
	for (const FString& SplitGroupName : CachedMeshPart.SplitGroups)
	{
		const EHoudiniSplitType SplitType = FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(SplitGroupName);
		if (SplitType == EHoudiniSplitType::Normal)
		{
			SplitToUse = SplitGroupName;
			break;
		}
		else if (SplitType == EHoudiniSplitType::LOD && SplitToUse.IsEmpty())
		{
			SplitToUse = SplitGroupName;
			// don't break here since we might still find normal geo after the LOD splits
		}
	}

	if (!SplitToUse.IsEmpty())
	{
		FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, InOutCachedMeshPart, SplitToUse, Attributes, Tokens);
	}
	else
	{
		FHoudiniMeshTranslatorUtils::CopyAttributesFromHGPOForSplit(HGPO, 0, 0, Attributes, Tokens);
	}
	
	FHoudiniEngineUtils::UpdatePackageParamsForTempOutputWithResolver(
		InOutPackageParams,
		IsValid(InOuterComponent) ? InOuterComponent->GetWorld() : nullptr,
		InOuterComponent,
		Attributes,
		Tokens,
		FinalPackageParams,
		Resolver);
	
	TArray<UPackage*> MaterialAndTexturePackages;

	TArray<UMaterialInterface*> OutMaterialArray;

	FinalPackageParams.OverideEnabled = false;
	FHoudiniMaterialTranslator::CreateHoudiniMaterials(
		HGPO.AssetId,
		FinalPackageParams,
		CachedMeshPart.GetUniqueMaterialIds(HGPO),
		CachedMeshPart.GetUniqueMaterialInfos(HGPO),
		InInputAssignmentMaterials,
		InAllOutputMaterials,
		InOutOutputAssignmentMaterials,
		OutMaterialArray,
		MaterialAndTexturePackages,
		false, 
		InTranslationContext.bTreatExistingMaterialsAsUpToDate);

	// See if we have material attributes that we want to apply to the generated materials
	TArray<FHoudiniGenericAttribute> MatAttributes;
	FHoudiniMaterialTranslator::GetMaterialParameterAttributes(HGPO.GeoId, HGPO.PartId, HAPI_ATTROWNER_PRIM, MatAttributes);
	if (MatAttributes.Num() > 0)
	{
		for (auto& CurMat : OutMaterialArray)
		{
			FHoudiniEngineUtils::UpdateGenericPropertiesAttributes(
				CurMat, MatAttributes, 0, true);
		}
	}

	if (CachedMeshPart.NeedsMaterialOverrideInstance(HGPO) && CachedMeshPart.GetFaceMaterialOverrides(HGPO).Num() > 0)
	{
		// Map containing unique face materials override attribute
		// and their first valid prim index
		// We create only one material instance per attribute

		FHoudiniMaterialTranslator::SortUniqueFaceMaterialOverridesAndCreateMaterialInstances(CachedMeshPart.GetFaceMaterialOverrides(HGPO), HGPO, InOutPackageParams, MaterialAndTexturePackages,
			InInputAssignmentMaterials, InOutOutputAssignmentMaterials,
			false);
	}

	return true;
}

bool
FHoudiniMeshTranslatorUtils::HavePartMaterialsChanged(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart)
{
	for (const auto& MaterialInfo : InOutCachedMeshPart.GetUniqueMaterialInfos(InHGPO))
	{
		if (MaterialInfo.hasChanged)
			return true;
	}

	return false;
}

bool
FHoudiniMeshTranslatorUtils::ExtractMaterialIndex(FString& MaterialName, int32& MatIndex)
{
	// Attempt to extract an index
	if (MaterialName.StartsWith("["))
	{
		int32 End = MaterialName.Find("]");
		if (End != -1)
		{
			FString Fragment = MaterialName.Mid(1, End - 1);
			if (FCString::IsNumeric(*Fragment))
			{
				MatIndex = FCString::Atoi(*Fragment);
				MaterialName = MaterialName.Mid(End + 1);
			}
		}
	}
	// correct invalid index
	if (MatIndex < -1)
	{
		MaterialName = "[" + FString::FromInt(MatIndex) + "]" + MaterialName;
		MatIndex = -1;
	}

	return true;
}

int32
FHoudiniMeshTranslatorUtils::GetFaceMaterialIndex(UMaterialInterface* MaterialInterface,
	TMap<TObjectPtr<UMaterialInterface>, int32>& MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh,
	int32 MatIndex,
	TArray<FStaticMaterial>& FoundStaticMaterials)
{
	int32 CurrentFaceMaterialIdx = 0;
	// only rely on our map if no index specified in the attribute value
	int32 const* FoundFaceMaterialIdx = MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Find(MaterialInterface);
	if (FoundFaceMaterialIdx && MatIndex == -1)
	{
		// We already know what material index to use for that override
		CurrentFaceMaterialIdx = *FoundFaceMaterialIdx;
	}
	else
	{
		// assign a default index for this mat
		if (MatIndex == -1)
		{
			CurrentFaceMaterialIdx = FoundStaticMaterials.Add(FStaticMaterial(MaterialInterface));
			MapUnrealMaterialInterfaceToUnrealMaterialIndexThisMesh.Add(MaterialInterface, CurrentFaceMaterialIdx);
		}
		// else, there is a specific index specified in the attrib value
		else
		{
			// add the mat to the mesh's material list if needed
			if (FoundStaticMaterials.Num() <= MatIndex || FoundStaticMaterials[MatIndex].MaterialInterface == nullptr)
			{
				FoundStaticMaterials.Reserve(MatIndex + 1);
				// pad with empty materials
				while (FoundStaticMaterials.Num() <= MatIndex)
				{
					FoundStaticMaterials.Add(FStaticMaterial());
				}
				FoundStaticMaterials[MatIndex] = FStaticMaterial(MaterialInterface);
			}
			CurrentFaceMaterialIdx = MatIndex;
		}
	}

	return CurrentFaceMaterialIdx;
}

FString
FHoudiniMeshTranslatorUtils::GetMeshIdentifierFromSplit(const FString& InSplitName, EHoudiniSplitType InSplitType)
{
	FString MeshIdentifier = TEXT("");
	switch (InSplitType)
	{
		case EHoudiniSplitType::Normal:
		case EHoudiniSplitType::LOD:
		case EHoudiniSplitType::InvisibleUCXCollider:
		case EHoudiniSplitType::InvisibleSimpleCollider:
			// LODs and Invisible simple colliders use the main mesh
			MeshIdentifier = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
			break;

		case EHoudiniSplitType::InvisibleComplexCollider:
		case EHoudiniSplitType::RenderedComplexCollider:
		case EHoudiniSplitType::RenderedUCXCollider:
		case EHoudiniSplitType::RenderedSimpleCollider:
			// Rendered colliders or invisible complex colliders have their own static mesh
			MeshIdentifier = InSplitName;
			break;

		default:
			break;
	}

	return MeshIdentifier;
}

EHoudiniSplitType
FHoudiniMeshTranslatorUtils::GetSplitTypeFromSplitName(const FString& InSplitName)
{
	const FString MainGroup = HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION;
	if (InSplitName.StartsWith(MainGroup, ESearchCase::IgnoreCase))
		return EHoudiniSplitType::Normal;

	const FString LODGroupPrefix = HAPI_UNREAL_GROUP_LOD_PREFIX;
	if (InSplitName.StartsWith(LODGroupPrefix, ESearchCase::IgnoreCase))
	{
		return EHoudiniSplitType::LOD;
	}
		
	const FString RenderedCollisionPrefix = HAPI_UNREAL_GROUP_RENDERED_COLLISION_PREFIX;
	if (InSplitName.StartsWith(RenderedCollisionPrefix, ESearchCase::IgnoreCase))
	{
		// Rendered colliders
		// See if it is a simple/ucx/complex
		const FString RenderedUCXCollisionPrefix = HAPI_UNREAL_GROUP_RENDERED_UCX_COLLISION_PREFIX;
		const FString RenderedSimpleCollisionPrefix = HAPI_UNREAL_GROUP_RENDERED_SIMPLE_COLLISION_PREFIX;
		if (InSplitName.StartsWith(RenderedUCXCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::RenderedUCXCollider;
		}
		else if (InSplitName.StartsWith(RenderedSimpleCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::RenderedSimpleCollider;
		}
		else
		{
			return EHoudiniSplitType::RenderedComplexCollider;
		}
	}

	const FString InvisibleCollisionPrefix = HAPI_UNREAL_GROUP_INVISIBLE_COLLISION_PREFIX;
	if (InSplitName.StartsWith(InvisibleCollisionPrefix, ESearchCase::IgnoreCase))
	{
		// Invisible colliders
		// See if it is a simple/ucx/complex
		const FString InvisibleUCXCollisionPrefix = HAPI_UNREAL_GROUP_INVISIBLE_UCX_COLLISION_PREFIX;
		const FString InvisibleSimpleCollisionPrefix = HAPI_UNREAL_GROUP_INVISIBLE_SIMPLE_COLLISION_PREFIX;
		if (InSplitName.StartsWith(InvisibleUCXCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::InvisibleUCXCollider;
		}
		else if (InSplitName.StartsWith(InvisibleSimpleCollisionPrefix, ESearchCase::IgnoreCase))
		{
			return EHoudiniSplitType::InvisibleSimpleCollider;
		}
		else
		{
			return EHoudiniSplitType::InvisibleComplexCollider;
		}
	}

	// ?
	return EHoudiniSplitType::Invalid;
	//return EHoudiniSplitType::Normal;
}

TArray<FVector> FHoudiniMeshTranslatorUtils::GetKdopDirections(const FString& SplitGroupName)
{
	uint32 NumDirections = 26;
	const FVector* Directions = KDopDir26;
	if (SplitGroupName.Contains("kdop10X"))
	{
		NumDirections = 10;
		Directions = KDopDir10X;
	}
	else if (SplitGroupName.Contains("kdop10Y"))
	{
		NumDirections = 10;
		Directions = KDopDir10Y;
	}
	else if (SplitGroupName.Contains("kdop10Z"))
	{
		NumDirections = 10;
		Directions = KDopDir10Z;
	}
	else if (SplitGroupName.Contains("kdop18"))
	{
		NumDirections = 18;
		Directions = KDopDir18;
	}

	// Converting the directions to a TArray
	TArray<FVector> DirArray;
	DirArray.SetNum(NumDirections);
	for (uint32 DirectionIndex = 0; DirectionIndex < NumDirections; DirectionIndex++)
	{
		DirArray[DirectionIndex] = Directions[DirectionIndex];
	}
	return DirArray;
}

int32
FHoudiniMeshTranslatorUtils::TransferRegularPointAttributesToVertices(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<float>& OutVertexData)
{
	return FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
		InVertexList, InAttribInfo,	InData,	OutVertexData);
}

template <typename TYPE>
int32 FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<TYPE>& InData,
	TArray<TYPE>& OutVertexData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit);

	if (!InAttribInfo.exists || InAttribInfo.tupleSize <= 0)
		return 0;

	if (InData.Num() <= 0)
		return 0;

	int32 ValidWedgeCount = 0;

	// Future optimization - see if we can do direct vertex transfer.
	int32 WedgeCount = InVertexList.Num();
	int32 LastValidWedgeIdx = 0;
	if (InAttribInfo.owner == HAPI_ATTROWNER_POINT)
	{		
		// Point attribute transfer
		OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			int32 VertexIdx = InVertexList[WedgeIdx];
			if (VertexIdx < 0)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
			for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
			{
				OutVertexData[OutIdx + TupleIdx] = InData[VertexIdx * InAttribInfo.tupleSize + TupleIdx];
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;
		}
	}
	else if (InAttribInfo.owner == HAPI_ATTROWNER_VERTEX)
	{
		// Vertex attribute transfer
		OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			if (InVertexList[WedgeIdx] < 0)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
			for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
			{				
				OutVertexData[OutIdx + TupleIdx] = InData[WedgeIdx * InAttribInfo.tupleSize + TupleIdx];
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;
		}
	}
	else if (InAttribInfo.owner == HAPI_ATTROWNER_PRIM)
	{
		// Primitive attribute transfer
		OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
		{
			if (InVertexList[WedgeIdx] < 0)
			{
				// This is an index/wedge we are skipping due to split.
				continue;
			}

			int32 PrimIdx = WedgeIdx / 3;
			int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
			for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
			{
				OutVertexData[OutIdx + TupleIdx] = InData[PrimIdx * InAttribInfo.tupleSize + TupleIdx];
			}

			// We are re-indexing wedges.
			LastValidWedgeIdx++;
			// Increment wedge count, since this is a valid wedge.
			ValidWedgeCount++;
		}
	}
	else if (InAttribInfo.owner == HAPI_ATTROWNER_DETAIL)
	{
		// Detail attribute transfer
		// We have one value to copy for all output split vertices
		// if the attribute is a single value (not a tuple)
		// then we can simply use the array init function instead of looping
		if (InAttribInfo.tupleSize == 1)
		{
			OutVertexData.SetNumUninitialized(WedgeCount);
			for (int32 n = 0; n < OutVertexData.Num(); n++)
				OutVertexData[n] = InData[0];
		}
		else
		{
			OutVertexData.SetNumZeroed(WedgeCount * InAttribInfo.tupleSize);
			for (int32 WedgeIdx = 0; WedgeIdx < WedgeCount; ++WedgeIdx)
			{
				if (InVertexList[WedgeIdx] < 0)
				{
					// This is an index/wedge we are skipping due to split.
					continue;
				}

				int32 OutIdx = LastValidWedgeIdx * InAttribInfo.tupleSize;
				for (int32 TupleIdx = 0; TupleIdx < InAttribInfo.tupleSize; TupleIdx++)
				{
					OutVertexData[OutIdx + TupleIdx] = InData[TupleIdx];
				}

				// We are re-indexing wedges.
				LastValidWedgeIdx++;
				// Increment wedge count, since this is a valid wedge.
				ValidWedgeCount++;
			}
		}
	}
	else
	{
		// Invalid attribute owner, shouldn't happen
		check(false);
	}

	OutVertexData.SetNumZeroed(ValidWedgeCount * InAttribInfo.tupleSize);

	return ValidWedgeCount;
}

bool
FHoudiniMeshTranslatorUtils::TryToFindPropertyOnSourceModel(
	UStaticMesh* const InStaticMesh,
	int32 InSourceModelIndex,
	const FString& InPropertyName,
	FEditPropertyChain& InPropertyChain,
	bool& bOutSkipDefaultIfPropertyNotFound,
	FProperty*& OutFoundProperty,
	UObject*& OutFoundPropertyObject,
	void*& OutContainer
	)
{
	if (!IsValid(InStaticMesh))
		return false;

	if (!InStaticMesh->IsSourceModelValid(InSourceModelIndex))
		return false;

	FStaticMeshSourceModel& SourceModel = InStaticMesh->GetSourceModel(InSourceModelIndex);

	// Restrict to only applying properties to the source model -- if the property is not found on the
	// source model, don't search via the static mesh
	bOutSkipDefaultIfPropertyNotFound = true;
	FProperty* const SourceModelsProperty = InStaticMesh->GetClass()->FindPropertyByName(FName(TEXT("SourceModels")));
	if (SourceModelsProperty)
		InPropertyChain.AddTail(SourceModelsProperty);

	if (!TryToFindPropertyOnSourceModel(SourceModel, InPropertyName, InPropertyChain, OutFoundProperty, OutContainer))
		return false;

	OutFoundPropertyObject = InStaticMesh;
	return true;
}

bool
FHoudiniMeshTranslatorUtils::TryToFindPropertyOnSourceModel(
	FStaticMeshSourceModel& InSourceModel,
	const FString& InPropertyName,
	FEditPropertyChain& InPropertyChain,
	FProperty*& OutFoundProperty,
	void*& OutContainer)
{
	bool bFoundProperty = false;
	FHoudiniGenericAttribute::TryToFindProperty(
		&InSourceModel, InSourceModel.StaticStruct(), InPropertyName, InPropertyChain, OutFoundProperty, bFoundProperty, OutContainer, false);
	return bFoundProperty;
}

float
FHoudiniMeshTranslatorUtils::GetLODSCreensizeForSplit(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FString& SplitGroupName)
{
	FHoudiniCachedMeshPart& CachedMeshPart = InOutCachedMeshPart;
	// LOD Screensize
	// default values has already been set, see if we have any attribute override for this
	float screensize = -1.0f;

	// Start by looking at the lod_screensize primitive attribute
	bool bAttribValid = false;
	if (CachedMeshPart.GetLODScreensizes(InHGPO).Num() > 0)
	{
		// use the "lod_screensize" primitive attribute
		int32 FirstValidPrimIndex = CachedMeshPart.Splits.FindChecked(SplitGroupName).FirstValidPrimIndex;
		if (CachedMeshPart.GetLODScreensizes(InHGPO).IsValidIndex(FirstValidPrimIndex))
			screensize = CachedMeshPart.GetLODScreensizes(InHGPO)[FirstValidPrimIndex];
	}
	
	if (screensize < 0.0f)
	{
		// We couldn't find the primitive attribute, look for a "lodX_screensize" detail attribute
		FString LODAttributeName = SplitGroupName + HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE_POSTFIX;

		TArray<float> LODScreenSizes;

		FHoudiniHapiAccessor Accessor(InHGPO.GeoInfo.NodeId, InHGPO.PartInfo.PartId, TCHAR_TO_ANSI(*LODAttributeName));
		bool bSuccess = Accessor.GetAttributeData(HAPI_ATTROWNER_DETAIL, LODScreenSizes, 0, 1);

		if (bSuccess)
		{
			screensize = LODScreenSizes[0];
		}			
	}

	if (screensize < 0.0f)
	{
		// finally, look for a potential uproperty style attribute
		// aka, "unreal_uproperty_screensize"
		TArray<float> LODScreenSizes;
		HAPI_AttributeInfo AttribInfoScreenSize;
		FHoudiniApi::AttributeInfo_Init(&AttribInfoScreenSize);

		FHoudiniHapiAccessor Accessor(InHGPO.GeoInfo.NodeId, InHGPO.PartInfo.PartId, HAPI_UNREAL_ATTRIB_LOD_SCREENSIZE);
		bool bSuccess = Accessor.GetInfo(AttribInfoScreenSize);

		if (bSuccess)
			Accessor.GetAttributeData(AttribInfoScreenSize, LODScreenSizes, 0, 1);

		if (bSuccess && AttribInfoScreenSize.exists)
		{
			if (AttribInfoScreenSize.owner == HAPI_ATTROWNER_DETAIL && LODScreenSizes.Num() > 0)
			{
				screensize = LODScreenSizes[0];
			}
			else if (AttribInfoScreenSize.owner == HAPI_ATTROWNER_PRIM)
			{
				int32 FirstValidPrimIndex = CachedMeshPart.Splits.FindChecked(SplitGroupName).FirstValidPrimIndex;
				if (LODScreenSizes.IsValidIndex(FirstValidPrimIndex))
					screensize = LODScreenSizes[FirstValidPrimIndex];
			}
		}
	}

	// Make sure the screensize is in percent, so if its above 1, divide by 100
	if (screensize > 1.0f)
		screensize /= 100.0f;

	return screensize;
}

int32 
FHoudiniMeshTranslatorUtils::GenerateBoxAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	// Calculate bounding Box.	
	FVector Center, Extents;
	FVector unitVec = FVector::OneVector;// bs->BuildScale3D;
	CalcBoundingBox(InPositionArray, Center, Extents, unitVec);

	FKBoxElem BoxElem;
	BoxElem.Center = Center;
	BoxElem.X = Extents.X * 2.0f;
	BoxElem.Y = Extents.Y * 2.0f;
	BoxElem.Z = Extents.Z * 2.0f;
	OutAggregateCollisions.BoxElems.Add(BoxElem);

	return 1;
}

int32 
FHoudiniMeshTranslatorUtils::GenerateOrientedBoxAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code adapted from Experimental GeometryProcessing plugin for simple mesh approximation.
	//

	const int32 NumPoints = InPositionArray.Num();
	TArray<houdini::gte::Vector3<double>> Points;
	Points.SetNumUninitialized(InPositionArray.Num());
	for(int32 i = 0; i < NumPoints; ++i)
	{
		Points[i] = Convert<double>(InPositionArray[i]);
	}
	// Calculate bounding Box.
	houdini::gte::OrientedBox3<double> MinimalBox = houdini::gte::OrientedBox3<double>();
	houdini::gte::MinimumVolumeBox3<double, double> BoxCompute;
	MinimalBox = BoxCompute(NumPoints, Points.GetData(), nullptr);
	
	// FVector unitVec = FVector::OneVector;// bs->BuildScale3D;
	// CalcBoundingBox(InPositionArray, Center, Extents, unitVec);
	
	const FVector X = Convert<double>(MinimalBox.axis[0]);
	const FVector Y = Convert<double>(MinimalBox.axis[1]);
	const FRotator Rot = FRotationMatrix::MakeFromXY(X,Y).Rotator();
	
	const FVector Extents = Convert(MinimalBox.extent);
	FKBoxElem BoxElem;
	BoxElem.Center = Convert(MinimalBox.center);
	BoxElem.X = Extents.X * 2.0f;
	BoxElem.Y = Extents.Y * 2.0f;
	BoxElem.Z = Extents.Z * 2.0f;
	BoxElem.Rotation = Rot;
	OutAggregateCollisions.BoxElems.Add(BoxElem);

	return 1;
}

void
FHoudiniMeshTranslatorUtils::CalcBoundingBox(const TArray<FVector>& PositionArray, FVector& Center, FVector& Extents, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FBox Box(ForceInit);
	for (const FVector& CurPos : PositionArray)
	{
		Box += CurPos;
	}
	Box.GetCenterAndExtents(Center, Extents);
}

int32
FHoudiniMeshTranslatorUtils::GenerateSphereAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FSphere bSphere, bSphere2, bestSphere;
	FVector unitVec = FVector::OneVector;

	// Calculate bounding sphere.
	CalcBoundingSphere(InPositionArray, bSphere, unitVec);
	CalcBoundingSphere2(InPositionArray, bSphere2, unitVec);

	if (bSphere.W < bSphere2.W)
		bestSphere = bSphere;
	else
		bestSphere = bSphere2;

	// Don't use if radius is zero.
	if (bestSphere.W <= 0.f)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple Sphere collider."));
		return 0;
	}

	FKSphereElem SphereElem;
	SphereElem.Center = bestSphere.Center;
	SphereElem.Radius = bestSphere.W;
	OutAggregateCollisions.SphereElems.Add(SphereElem);

	return 1;
}

void
FHoudiniMeshTranslatorUtils::CalcBoundingSphere(const TArray<FVector>& PositionArray, FSphere& sphere, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	if (PositionArray.Num() == 0)
		return;

	FBox Box;
	FVector MinIx[3] = {};
	FVector MaxIx[3] = {};

	bool bFirstVertex = true;
	for (const FVector& CurPosition : PositionArray)
	{
		FVector p = CurPosition * LimitVec;
		if (bFirstVertex)
		{
			// First, find AABB, remembering furthest points in each dir.
			Box.Min = p;
			Box.Max = Box.Min;

			MinIx[0] = CurPosition;
			MinIx[1] = CurPosition;
			MinIx[2] = CurPosition;

			MaxIx[0] = CurPosition;
			MaxIx[1] = CurPosition;
			MaxIx[2] = CurPosition;
			bFirstVertex = false;
			continue;
		}

		// X //
		if (p.X < Box.Min.X)
		{
			Box.Min.X = p.X;
			MinIx[0] = CurPosition;
		}
		else if (p.X > Box.Max.X)
		{
			Box.Max.X = p.X;
			MaxIx[0] = CurPosition;
		}

		// Y //
		if (p.Y < Box.Min.Y)
		{
			Box.Min.Y = p.Y;
			MinIx[1] = CurPosition;
		}
		else if (p.Y > Box.Max.Y)
		{
			Box.Max.Y = p.Y;
			MaxIx[1] = CurPosition;
		}

		// Z //
		if (p.Z < Box.Min.Z)
		{
			Box.Min.Z = p.Z;
			MinIx[2] = CurPosition;
		}
		else if (p.Z > Box.Max.Z)
		{
			Box.Max.Z = p.Z;
			MaxIx[2] = CurPosition;
		}
	}

	const FVector Extremes[3] = { (MaxIx[0] - MinIx[0]) * LimitVec,
		(MaxIx[1] - MinIx[1]) * LimitVec,
		(MaxIx[2] - MinIx[2]) * LimitVec };

	// Now find extreme points furthest apart, and initial center and radius of sphere.
	float d2 = 0.f;
	for (int32 i = 0; i < 3; i++)
	{
		const float tmpd2 = Extremes[i].SizeSquared();
		if (tmpd2 > d2)
		{
			d2 = tmpd2;
			sphere.Center = (MinIx[i] + (0.5f * Extremes[i])) * LimitVec;
			sphere.W = 0.f;
		}
	}

	const FVector Extents = FVector(Extremes[0].X, Extremes[1].Y, Extremes[2].Z);

	// radius and radius squared
	float r = 0.5f * Extents.GetMax();
	float r2 = FMath::Square(r);

	// Now check each point lies within this sphere. If not - expand it a bit.
	for (const FVector& curPos : PositionArray)
	{
		const FVector cToP = (curPos * LimitVec) - sphere.Center;

		const float pr2 = cToP.SizeSquared();

		// If this point is outside our current bounding sphere's radius
		if (pr2 > r2)
		{
			// ..expand radius just enough to include this point.
			const float pr = FMath::Sqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = FMath::Square(r);

			sphere.Center += ((pr - r) / pr * cToP);
		}
	}

	sphere.W = r;
}

void
FHoudiniMeshTranslatorUtils::CalcBoundingSphere2(const TArray<FVector>& PositionArray, FSphere& sphere, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FVector Center, Extents;
	CalcBoundingBox(PositionArray, Center, Extents, LimitVec);

	sphere.Center = Center;
	sphere.W = 0.0f;

	for (const FVector& curPos : PositionArray)
	{
		float Dist = FVector::DistSquared(curPos * LimitVec, sphere.Center);
		if (Dist > sphere.W)
			sphere.W = Dist;
	}
	sphere.W = FMath::Sqrt(sphere.W);
}

int32 
FHoudiniMeshTranslatorUtils::GenerateSphylAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	FSphere sphere;
	float length;
	FRotator rotation;
	FVector unitVec = FVector::OneVector;

	// Calculate bounding sphyl.
	CalcBoundingSphyl(InPositionArray, sphere, length, rotation, unitVec);

	// Dont use if radius is zero.
	if (sphere.W <= 0.f)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple Capsule collider."));
		return 0;
	}

	// If height is zero, then a sphere would be better (should we just create one instead?)
	if (length <= 0.f)
	{
		length = SMALL_NUMBER;
	}

	FKSphylElem SphylElem;
	SphylElem.Center = sphere.Center;
	SphylElem.Rotation = rotation;
	SphylElem.Radius = sphere.W;
	SphylElem.Length = length;
	OutAggregateCollisions.SphylElems.Add(SphylElem);

	return 1;
}

int32 
FHoudiniMeshTranslatorUtils::GenerateOrientedSphylAsSimpleCollision(const TArray<FVector>& InPositionArray, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	//
	// Code adapted from Experimental GeometryProcessing plugin for simple mesh approximation.
	//

	const int32 NumPoints = InPositionArray.Num();
	TArray<houdini::gte::Vector3<double>> Points;
	Points.SetNumUninitialized(InPositionArray.Num());
	for(int32 i = 0; i < NumPoints; ++i)
	{
		Points[i] = Convert<double>(InPositionArray[i]);
	}

	houdini::gte::Capsule3<double> FitCapsule;
	const bool bResultValid = GetContainer(NumPoints, Points.GetData(), FitCapsule);
	if (!bResultValid)
	{
		return 0;
	}

	houdini::gte::Vector3<double> GteCenter, GteDirection;
	double Extent;
	FitCapsule.segment.GetCenteredForm(GteCenter, GteDirection, Extent);
	const FVector Direction = Convert<double>(GteDirection);
	const FRotator Rot = FRotationMatrix::MakeFromZ(Direction).Rotator();

	// Dont use if radius is zero.
	if (FitCapsule.radius <= 0.f)
	{
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple Capsule collider."));
		return 0;
	}

	// If height is zero, then a sphere would be better (should we just create one instead?)
	if (Extent <= 0.f)
	{
		Extent = SMALL_NUMBER;
	}

	FKSphylElem SphylElem;
	SphylElem.Center = Convert<double>(GteCenter);
	SphylElem.Rotation = Rot;
	SphylElem.Radius = FitCapsule.radius;
	SphylElem.Length = Extent*2.f;
	OutAggregateCollisions.SphylElems.Add(SphylElem);

	return 1;
}

void
FHoudiniMeshTranslatorUtils::CalcBoundingSphyl(const TArray<FVector>& PositionArray, FSphere& sphere, float& length, FRotator& rotation, FVector& LimitVec)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	if (PositionArray.Num() == 0)
		return;

	FVector Center, Extents;
	CalcBoundingBox(PositionArray, Center, Extents, LimitVec);

	// @todo sphere.Center could perhaps be adjusted to best fit if model is non-symmetric on it's longest axis
	sphere.Center = Center;

	// Work out best axis aligned orientation (longest side)
	float Extent = Extents.GetMax();
	if (Extent == Extents.X)
	{
		rotation = FRotator(90.f, 0.f, 0.f);
		Extents.X = 0.0f;
	}
	else if (Extent == Extents.Y)
	{
		rotation = FRotator(0.f, 0.f, 90.f);
		Extents.Y = 0.0f;
	}
	else
	{
		rotation = FRotator(0.f, 0.f, 0.f);
		Extents.Z = 0.0f;
	}

	// Cleared the largest axis above, remaining determines the radius
	float r = Extents.GetMax();
	float r2 = FMath::Square(r);

	// Now check each point lies within this the radius. If not - expand it a bit.
	for (const FVector& CurPos : PositionArray)
	{
		FVector cToP = (CurPos * LimitVec) - sphere.Center;
		cToP = rotation.UnrotateVector(cToP);

		const float pr2 = cToP.SizeSquared2D();	// Ignore Z here...

		// If this point is outside our current bounding sphere's radius
		if (pr2 > r2)
		{
			// ..expand radius just enough to include this point.
			const float pr = FMath::Sqrt(pr2);
			r = 0.5f * (r + pr);
			r2 = FMath::Square(r);
		}
	}

	// The length is the longest side minus the radius.
	float hl = FMath::Max(0.0f, Extent - r);

	// Now check each point lies within the length. If not - expand it a bit.
	for (const FVector& CurPos : PositionArray)
	{
		FVector cToP = (CurPos * LimitVec) - sphere.Center;
		cToP = rotation.UnrotateVector(cToP);

		// If this point is outside our current bounding sphyl's length
		if (FMath::Abs(cToP.Z) > hl)
		{
			const bool bFlip = (cToP.Z < 0.f ? true : false);
			const FVector cOrigin(0.f, 0.f, (bFlip ? -hl : hl));

			const float pr2 = (cOrigin - cToP).SizeSquared();

			// If this point is outside our current bounding sphyl's radius
			if (pr2 > r2)
			{
				FVector cPoint;
				FMath::SphereDistToLine(cOrigin, r, cToP, (bFlip ? FVector(0.f, 0.f, 1.f) : FVector(0.f, 0.f, -1.f)), cPoint);

				// Don't accept zero as a valid diff when we know it's outside the sphere (saves needless retest on further iterations of like points)
				hl += FMath::Max(FMath::Abs(cToP.Z - cPoint.Z), 1.e-6f);
			}
		}
	}

	sphere.W = r;
	length = hl * 2.0f;
}

int32
FHoudiniMeshTranslatorUtils::GenerateKDopAsSimpleCollision(const TArray<FVector>& InPositionArray, const TArray<FVector>& Dirs, FKAggregateGeom& OutAggregateCollisions)
{
	//
	// Code simplified and adapted to work with a simple vector array from GeomFitUtils.cpp
	//

	const float my_flt_max = 3.402823466e+38F;

	// Do k- specific stuff.
	int32 kCount = Dirs.Num();

	TArray<float> maxDist;
	maxDist.SetNumUninitialized(kCount);
	for (int32 n = 0; n < maxDist.Num(); n++)
		maxDist[n] = -my_flt_max;

	// Construct temporary UModel for kdop creation. We keep no refs to it, so it can be GC'd.
	auto TempModel = NewObject<UModel>();
	TempModel->Initialize(nullptr, 1);

	// For each vertex, project along each kdop direction, to find the max in that direction.
	for (int32 i = 0; i < InPositionArray.Num(); i++)
	{
		for (int32 j = 0; j < kCount; j++)
		{
			float dist = InPositionArray[i] | Dirs[j];
			maxDist[j] = FMath::Max(dist, maxDist[j]);
		}
	}

	// Inflate kdop to ensure it is no degenerate
	const float MinSize = 0.1f;
	for (int32 i = 0; i < kCount; i++)
	{
		maxDist[i] += MinSize;
	}

	// Now we have the planes of the kdop, we work out the face polygons.
	TArray<FPlane4f> planes;
	for (int32 i = 0; i < kCount; i++)
		planes.Add(FPlane4f((FVector3f)Dirs[i], maxDist[i]));

	for (int32 i = 0; i < planes.Num(); i++)
	{
		FPoly*	Polygon = new(TempModel->Polys->Element) FPoly();
		FVector3f Base, AxisX, AxisY;

		Polygon->Init();
		Polygon->Normal = planes[i];
		Polygon->Normal.FindBestAxisVectors(AxisX, AxisY);

		Base = planes[i] * planes[i].W;

		new(Polygon->Vertices) FVector3f(Base + AxisX * UE_OLD_HALF_WORLD_MAX + AxisY * UE_OLD_HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector3f(Base + AxisX * UE_OLD_HALF_WORLD_MAX - AxisY * UE_OLD_HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector3f(Base - AxisX * UE_OLD_HALF_WORLD_MAX - AxisY * UE_OLD_HALF_WORLD_MAX);
		new(Polygon->Vertices) FVector3f(Base - AxisX * UE_OLD_HALF_WORLD_MAX + AxisY * UE_OLD_HALF_WORLD_MAX);

		for (int32 j = 0; j < planes.Num(); j++)
		{
			if (i != j)
			{
				if (!Polygon->Split(-FVector3f(planes[j]), planes[j] * planes[j].W))
				{
					Polygon->Vertices.Empty();
					break;
				}
			}
		}

		if (Polygon->Vertices.Num() < 3)
		{
			// If poly resulted in no verts, remove from array
			TempModel->Polys->Element.RemoveAt(TempModel->Polys->Element.Num() - 1);
		}
		else
		{
			// Other stuff...
			Polygon->iLink = i;
			Polygon->CalcNormal(1);
		}
	}

	if (TempModel->Polys->Element.Num() < 4)
	{
		TempModel = NULL;
		HOUDINI_LOG_WARNING(TEXT("Failed to generate a simple KDOP collider."));
		return 0;
	}

	// Build bounding box.
	TempModel->BuildBound();

	// Build BSP for the brush.
	FBSPOps::bspBuild(TempModel, FBSPOps::BSP_Good, 15, 70, 1, 0);
	FBSPOps::bspRefresh(TempModel, 1);
	FBSPOps::bspBuildBounds(TempModel);

	// Now, create a temporary BodySetup to build the colliders
	UBodySetup* TempBS = NewObject<UBodySetup>();
	TempBS->CreateFromModel(TempModel, false);

	// Copy the convex elements back to our aggregate
	int32 NewConvexElems = 0;
	if (TempBS && TempBS->AggGeom.ConvexElems.Num() > 0)
	{
		for (const auto& CurConvexElem : TempBS->AggGeom.ConvexElems)
		{
			OutAggregateCollisions.ConvexElems.Add(CurConvexElem);
			NewConvexElems++;
		}
	}

	return NewConvexElems;
}

bool
FHoudiniMeshTranslatorUtils::AddSimpleCollisionToAggregate(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FString& InSplitGroupName,
	FKAggregateGeom& OutAggregateCollisions)
{
	// Get the vertex indices for the split group.
	TArray<int32>& SplitGroupVertexList = InOutCachedMeshPart.Splits.FindChecked(InSplitGroupName).VertexList;

	// We're only interested in unique vertices.
	TArray<int32> UniqueVertexIndexes;
	for (int32 VertexIdx = 0; VertexIdx < SplitGroupVertexList.Num(); VertexIdx++)
	{
		const int32 Index = SplitGroupVertexList[VertexIdx];
		if (!InOutCachedMeshPart.GetPositions(InHGPO).IsValidIndex(Index))
			continue;

		UniqueVertexIndexes.AddUnique(Index);
	}

	// Extract the collision geometry's vertices.
	TArray<FVector> VertexArray;
	VertexArray.SetNum(UniqueVertexIndexes.Num());
	for (int32 Index = 0; Index < UniqueVertexIndexes.Num(); Index++)
	{
		const int32 VertexIndex = UniqueVertexIndexes[Index];
		if (!InOutCachedMeshPart.GetPositions(InHGPO).IsValidIndex(VertexIndex * 3 + 2))
			continue;

		VertexArray[Index].X = InOutCachedMeshPart.GetPositions(InHGPO)[VertexIndex * 3 + 0] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Index].Y = InOutCachedMeshPart.GetPositions(InHGPO)[VertexIndex * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		VertexArray[Index].Z = InOutCachedMeshPart.GetPositions(InHGPO)[VertexIndex * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
	}

	int32 NumNewColliders = 0;
	if (InSplitGroupName.Contains("Box", ESearchCase::IgnoreCase))
	{
		NumNewColliders = GenerateOrientedBoxAsSimpleCollision(VertexArray, OutAggregateCollisions);
	}
	else if (InSplitGroupName.Contains("Sphere", ESearchCase::IgnoreCase))
	{
		NumNewColliders = GenerateSphereAsSimpleCollision(VertexArray, OutAggregateCollisions);
	}
	else if (InSplitGroupName.Contains("Capsule", ESearchCase::IgnoreCase))
	{
		NumNewColliders = GenerateOrientedSphylAsSimpleCollision(VertexArray, OutAggregateCollisions);
	}
	else
	{
		const TArray<FVector> Directions = GetKdopDirections(InSplitGroupName);
		NumNewColliders = GenerateKDopAsSimpleCollision(VertexArray, Directions, OutAggregateCollisions);
	}

	return NumNewColliders > 0;
}

FKAggregateGeom
FHoudiniMeshTranslatorUtils::BuildAggregateCollision(
	const FHoudiniGeoPartObject& InHGPO,
	FHoudiniCachedMeshPart& InOutCachedMeshPart,
	const FHoudiniSplitGroupMesh& InMesh)
{
	FKAggregateGeom AggregateCollisions;
	for (const int32 Index : InMesh.SimpleCollisions)
	{
		const FHoudiniGroupedMeshPrimitives& SplitMesh = InMesh.SplitMeshData[Index];
		const bool bSuccess = AddSimpleCollisionToAggregate(
			InHGPO, InOutCachedMeshPart, SplitMesh.SplitGroupName, AggregateCollisions);
		if (!bSuccess)
			HOUDINI_LOG_WARNING(TEXT("Failed to create simple collider."));
	}

	return AggregateCollisions;
}

UMeshComponent*
FHoudiniMeshTranslatorUtils::CreateMeshComponent(UObject* InOuterComponent, const TSubclassOf<UMeshComponent>& InComponentType)
{
	// Create a new SMC as we couldn't find an existing one
	USceneComponent* OuterSceneComponent = Cast<USceneComponent>(InOuterComponent);
	UObject * Outer = nullptr;
	if (IsValid(OuterSceneComponent))
		Outer = OuterSceneComponent->GetOwner() ? OuterSceneComponent->GetOwner() : OuterSceneComponent->GetOuter();

	UMeshComponent *MeshComponent = NewObject<UMeshComponent>(Outer, InComponentType, NAME_None, RF_Transactional);

	// Initialize it
	MeshComponent->SetVisibility(true);
	//MeshComponent->SetMobility(Mobility);

	// TODO:
	// Property propagation: set the new SMC's properties to the HAC's current settings
	//CopyComponentPropertiesTo(MeshComponent);

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(OuterSceneComponent);
	if (PrimComponent)
	{
		// The outer component is a primitive, so we can copy the collision profile from the scene component.  
		MeshComponent->SetCollisionProfileName(PrimComponent->GetCollisionProfileName());
	}

	// Change the creation method so the component is listed in the details panels
	FHoudiniEngineRuntimeUtils::AddOrSetAsInstanceComponent(MeshComponent);

	// Attach created static mesh component to our Houdini component.
	MeshComponent->AttachToComponent(OuterSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
	MeshComponent->OnComponentCreated();
	if(MeshComponent->GetOwner())
		MeshComponent->RegisterComponent();

	return MeshComponent;
}

UMeshComponent*
FHoudiniMeshTranslatorUtils::CreateOrUpdateMeshComponent(
	const UHoudiniOutput* InOutput,
	UObject* InOuterComponent,
	const FHoudiniOutputObjectIdentifier& InOutputIdentifier,
	const TSubclassOf<UMeshComponent>& InComponentType,
	FHoudiniOutputObject& OutputObject,
	const FHoudiniGeoPartObject*& OutFoundHGPO,
	bool& bCreated)
{
	bCreated = false;
	OutFoundHGPO = nullptr;

	// Find the HGPO that matches this mesh
	for (const FHoudiniGeoPartObject& curHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		if (curHGPO.ObjectId != InOutputIdentifier.ObjectId
			|| curHGPO.GeoId != InOutputIdentifier.GeoId
			|| curHGPO.PartId != InOutputIdentifier.PartId)
		{
			continue;
		}

		if (InOutputIdentifier.SplitIdentifier.Equals(HAPI_UNREAL_GROUP_GEOMETRY_NOT_COLLISION)
			|| curHGPO.SplitGroups.Contains(InOutputIdentifier.SplitIdentifier))
		{
			OutFoundHGPO = &curHGPO;
			break;
		}

		if (InOutput->GetType() == EHoudiniOutputType::Skeletal)
		{
			OutFoundHGPO = &curHGPO;
			break;
		}
	}

	// Instanced static meshes are represented by the instance translator. Dynamic Mesh outputs
	// need a component for the packed mesh itself, as they cannot be represented by static instances.
	bool bSkipInstancedComponent = OutFoundHGPO && OutFoundHGPO->bIsInstanced;
	if (bSkipInstancedComponent)
	{
		bool bIsDynamicMeshComponent = InComponentType && InComponentType->IsChildOf(UDynamicMeshComponent::StaticClass());
		if (!bIsDynamicMeshComponent)
			return nullptr;
	}

	bool bIsProxyComponent = InComponentType == UHoudiniStaticMeshComponent::StaticClass();

	// See if we already have a component for that mesh
	UMeshComponent* MeshComponent = nullptr;
	if (bIsProxyComponent) 
	{
		MeshComponent = Cast<UMeshComponent>(OutputObject.ProxyComponent);
	} 
	else if (OutputObject.OutputComponents.Num() > 0) 
	{
		MeshComponent = Cast<UMeshComponent>(OutputObject.OutputComponents[0]);
	}

	// If there is an existing component, but it is pending kill, then it was likely
	// deleted by some other process, such as by the user in the editor, so don't use it
	if ((!MeshComponent || !IsValid(MeshComponent)) || !MeshComponent->IsA(InComponentType))
	{
		// If the component is not of type InComponentType, or the found component is pending kill, destroy 
		// the existing component (a new one is then created below)
		FHoudiniEngineUtils::RemoveAndDestroyComponent(MeshComponent);
		MeshComponent = nullptr;
	}

	if (!MeshComponent)
	{
		// Create a new SMC/HSMC as we couldn't find an existing one
		MeshComponent = CreateMeshComponent(InOuterComponent, InComponentType);

		if (MeshComponent)
		{
			// Add to the output object
			if (bIsProxyComponent)
				OutputObject.ProxyComponent = MeshComponent;
			else
			{
				check(OutputObject.OutputComponents.Num() < 2); // Multiple components not supported yet.
				OutputObject.OutputComponents.Empty();
				OutputObject.OutputComponents.Add(MeshComponent);
			}

			bCreated = true;
		}
	}

	return MeshComponent;
}

void
FHoudiniMeshTranslatorUtils::UpdateMeshBuildSettings(
	const FMeshBuildSettings& InStaticMeshBuildSettings,
	FMeshBuildSettings& OutMeshBuildSettings,
	bool bHasNormals, 
	bool bHasTangents, 
	bool bHasLightmapUVSet)
{
	// Use the values provided to the translator
	OutMeshBuildSettings = InStaticMeshBuildSettings;

	const UHoudiniRuntimeSettings* HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();

	// Recomputing normals.
	EHoudiniRuntimeSettingsRecomputeFlag RecomputeNormalFlag = HoudiniRuntimeSettings ? (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeNormalsFlag : HRSRF_OnlyIfMissing;
	if(RecomputeNormalFlag == HRSRF_OnlyIfMissing)
		OutMeshBuildSettings.bRecomputeNormals = !bHasNormals;

	// Recomputing tangents.
	EHoudiniRuntimeSettingsRecomputeFlag RecomputeTangentFlag = HoudiniRuntimeSettings ? (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->RecomputeTangentsFlag : HRSRF_OnlyIfMissing;
	if (RecomputeTangentFlag == HRSRF_OnlyIfMissing)
		OutMeshBuildSettings.bRecomputeTangents = !bHasTangents;

	// Lightmap UV generation.
	EHoudiniRuntimeSettingsRecomputeFlag GenerateLightmapUVsFlag = HoudiniRuntimeSettings ? (EHoudiniRuntimeSettingsRecomputeFlag)HoudiniRuntimeSettings->GenerateLightmapUVsFlag : HRSRF_OnlyIfMissing;
	if (GenerateLightmapUVsFlag == HRSRF_OnlyIfMissing)
		OutMeshBuildSettings.bGenerateLightmapUVs = !bHasLightmapUVSet;
}

void FHoudiniMeshTranslatorUtils::UpdateSplitGroups(FHoudiniGeoPartObject& HGPO)
{
	// The old code (per-split groups) uses slightly different conditions to fill in the HGPO.SplitGroups. This function
	// fetches the groups using the new method.

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HAPI_Result Error = HAPI_RESULT_FAILURE;
	Error = FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), HGPO.GeoId, HGPO.PartId, &PartInfo);

	TArray<FString> GroupNames;
	if (!FHoudiniEngineUtils::HapiGetGroupNames(HGPO.GeoId, HGPO.PartId, HAPI_GROUPTYPE_PRIM, PartInfo.isInstanced, GroupNames))
	{
		return;
	}

	TArray<FString> Results;

	for (const FString& GroupName : GroupNames)
	{
		if (GroupName.StartsWith(HAPI_UNREAL_GROUP_LOD_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_INVISIBLE_COLLISION_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_RENDERED_COLLISION_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_RENDERED_PREFIX, ESearchCase::IgnoreCase))
		{
			// Split by collisions / lods
			Results.Add(GroupName);
		}
	}

	HGPO.SplitGroups = Results;
}

bool 
FHoudiniMeshTranslatorUtils::IsGammaCorrectionDisabled(HAPI_NodeId  NodeId, HAPI_PartId PartId)
{
	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_DISABLE_GAMMA_CORRECTION);
	TArray<int> Values;
	Accessor.GetAttributeData(HAPI_ATTROWNER_INVALID, Values);
	if (Values.IsEmpty())
		return false;

	return Values[0] != 0;
}

void
FHoudiniMeshTranslatorUtils::GetCustomPrimitiveData(
	int32 InGeoNodeId,
	int32 InPartId,
	int32 InPrimIndex,
	TArray<float>& OutCustomPrimData,
	HAPI_AttributeOwner InAttribOwner)
{
	// Get the number of custom primitive data
	TArray<int32> NumCustomPrimDataArray;
	FHoudiniHapiAccessor Accessor(InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_NUM_CUSTOM_PRIM_DATA);
	Accessor.GetAttributeData(InAttribOwner, NumCustomPrimDataArray, InPrimIndex, 1);
	if (NumCustomPrimDataArray.IsEmpty())
		return;

	int32 NumCustomPrimData = NumCustomPrimDataArray[0];
	OutCustomPrimData.SetNum(NumCustomPrimData);
	for (int CustomPrimDataIndex = 0; CustomPrimDataIndex < NumCustomPrimData; CustomPrimDataIndex++)
	{
		TArray<float> Values;
		FString CurrentAttr = TEXT(HAPI_UNREAL_ATTRIB_CUSTOM_PRIMITIVE_DATA_PREFIX) + FString::FromInt(CustomPrimDataIndex);
		Accessor.Init(InGeoNodeId, InPartId, TCHAR_TO_ANSI(*CurrentAttr));
		Accessor.GetAttributeData(InAttribOwner, Values, InPrimIndex, 1);
		if (Values.IsEmpty())
		{
			HOUDINI_LOG_ERROR(TEXT("Could not find attribute '%s' "), *CurrentAttr);
			continue;
		}

		OutCustomPrimData[CustomPrimDataIndex] = Values[0];
	}
}

void
FHoudiniMeshTranslatorUtils::SetCustomPrimitiveData(
	const TArray<float>& CustomPrimData,
	USceneComponent* InComponentToUpdate)
{
	if (CustomPrimData.IsEmpty())
		return;

	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(InComponentToUpdate);
	if (!IsValid(PrimComp))
		return;

	PrimComp->Modify();

	// Set both the custom primitive data array AND the default (details UI) array
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 5
	PrimComp->SetCustomPrimitiveDataFloatArray(0, CustomPrimData);
	PrimComp->SetDefaultCustomPrimitiveDataFloatArray(0, CustomPrimData);
#else
	for (int Idx = 0; Idx < CustomPrimData.Num(); Idx++)
	{
		PrimComp->SetCustomPrimitiveDataFloat(Idx, CustomPrimData[Idx]);
		PrimComp->SetDefaultCustomPrimitiveDataFloat(Idx, CustomPrimData[Idx]);
	}
#endif

	PrimComp->MarkRenderStateDirty();

	return;
}

template int32 FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<float>(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<float>& InData,
	TArray<float>& OutSplitData);

template int32 FHoudiniMeshTranslatorUtils::TransferPartAttributesToSplit<int32>(
	const TArray<int32>& InVertexList,
	const HAPI_AttributeInfo& InAttribInfo,
	const TArray<int32>& InData,
	TArray<int32>& OutSplitData);

#undef LOCTEXT_NAMESPACE
