#include "HoudiniGeometryCollectionTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniInstanceTranslator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionDebugDrawComponent.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionActor.h"
#include "Materials/Material.h"


void FHoudiniGeometryCollectionTranslator::SetupGeometryCollectionComponentFromOutputs(
	TArray<UHoudiniOutput*>& InAllOutputs, UObject* InOuterComponent, const FHoudiniPackageParams& InPackageParams, UWorld * InWorld)
{
	USceneComponent* ParentComponent = Cast<USceneComponent>(InOuterComponent);
	if (!ParentComponent)
		return;

	TMap<FString, FHoudiniGeometryCollectionData> GeometryCollectionData;
	GetGeometryCollectionData(InAllOutputs, InPackageParams, GeometryCollectionData);

	for (auto & Pair : GeometryCollectionData)
	{
		FString& GCName = Pair.Key;
		FHoudiniGeometryCollectionData& GCData = Pair.Value;

		// Find or create new UHoudiniOutput
		UHoudiniOutput * HoudiniOutput = nullptr;
		bool bNewOutput = true;
		
		// Find UHoudiniOutput
		for (auto& Output : InAllOutputs)
		{
			if (Output->Type != EHoudiniOutputType::GeometryCollection)
			{
				continue;
			}
	
			if (!Output->GetOutputObjects().Contains(GCData.Identifier))
			{
				continue;
			}
	
			HoudiniOutput = Output;
			bNewOutput = false;
			break;
		}
	
		if (!HoudiniOutput)
		{
			// Create the actual HoudiniOutput.
			HoudiniOutput = NewObject<UHoudiniOutput>(
			InOuterComponent,
			UHoudiniOutput::StaticClass(),
			FName(*GCName), // Used for baking identification
			RF_NoFlags);
	
			// Make sure the created object is valid 
			if (!IsValid(HoudiniOutput))
			{
				HOUDINI_LOG_WARNING(TEXT("Failed to create asset output"));
				return;
			}
	
			HoudiniOutput->Type = EHoudiniOutputType::GeometryCollection;
			InAllOutputs.Add(HoudiniOutput);
		}
	
		FHoudiniOutputObject& OutputObject = HoudiniOutput->OutputObjects.FindOrAdd(GCData.Identifier);
		
		TArray<FHoudiniGeometryCollectionPiece>& GeometryCollectionPieces = GCData.GeometryCollectionPieces;
		if (GeometryCollectionPieces.Num() <= 0)
		{
			return;
		}
	
		FString AssetName = ParentComponent->GetOwner()->GetName() + "_" + GCName;
		FString ActorName = AssetName + "_Actor";
		
		// Initialize GC, GC Actor, GC Component
		FTransform AssetTransform = ParentComponent->GetOwner()->GetTransform();
	
		UGeometryCollection* GeometryCollection = GCData.PackParams.CreateObjectAndPackage<UGeometryCollection>();
		if (!IsValid(GeometryCollection))
			return;
		
		AGeometryCollectionActor * GeometryCollectionActor = Cast<AGeometryCollectionActor>(OutputObject.OutputObject);
		
		if (!GeometryCollectionActor)
			GeometryCollectionActor = CreateNewGeometryActor(InWorld, ActorName, AssetTransform);
	
		if (!IsValid(GeometryCollectionActor))
			return;
	
		UGeometryCollectionComponent*  GeometryCollectionComponent = GeometryCollectionActor->GetGeometryCollectionComponent();
		if (!IsValid(GeometryCollectionComponent))
			return;
	
		GeometryCollectionActor->GetGeometryCollectionComponent()->SetRestCollection(GeometryCollection);
	
		UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(HoudiniOutput);
		if (IsValid(HAC))
		{
			GeometryCollectionActor->AttachToComponent(HAC, FAttachmentTransformRules::KeepWorldTransform);
		}
		
		// Mark relevant stuff dirty
		FAssetRegistryModule::AssetCreated(GeometryCollection);
		GeometryCollection->MarkPackageDirty();
	
		UPackage *OuterPackage = GeometryCollection->GetOutermost();
		if (IsValid(OuterPackage))
		{
			OuterPackage->MarkPackageDirty();
		}
		
		const FTransform ActorTransform(ParentComponent->GetOwner()->GetTransform());
	
		// Used to get the number of levels
		// Pair of <FractureIndex, ClusterIndex>
		TMap<TPair<int32, int32>, TArray<FHoudiniGeometryCollectionPiece *>> Clusters;
	
		// Append the static meshes build from instancers to the UGeometryCollection, destroying the StaticMeshComponents as you go
		// Kind of similar to UFractureToolGenerateAsset::ConvertStaticMeshToGeometryCollection
		for (auto & GeometryCollectionPiece : GeometryCollectionPieces)
		{
			if (!GeometryCollectionPiece.InstancerOutput->OutputComponent->IsA(UStaticMeshComponent::StaticClass()))
			{
				continue;
			}
	
			TPair<int32, int32> ClusterKey = TPair<int32, int32>(GeometryCollectionPiece.FractureIndex, GeometryCollectionPiece.ClusterIndex);
			TArray<FHoudiniGeometryCollectionPiece *> & Cluster = Clusters.FindOrAdd(ClusterKey);
			Cluster.Add(&GeometryCollectionPiece);
			
			UObject * OldComponent = GeometryCollectionPiece.InstancerOutput->OutputComponent;
			
			UStaticMeshComponent * StaticMeshComponent = Cast<UStaticMeshComponent>(GeometryCollectionPiece.InstancerOutput->OutputComponent);
			UStaticMesh * ComponentStaticMesh = StaticMeshComponent->GetStaticMesh();
			FTransform ComponentTransform(StaticMeshComponent->GetComponentTransform());
			ComponentTransform.SetTranslation(ComponentTransform.GetTranslation() - ActorTransform.GetTranslation());
			FSoftObjectPath SourceSoftObjectPath(ComponentStaticMesh);
			decltype(FGeometryCollectionSource::SourceMaterial) SourceMaterials(StaticMeshComponent->GetMaterials());
			GeometryCollection->GeometrySource.Add({ SourceSoftObjectPath, ComponentTransform, SourceMaterials });
			AppendStaticMesh(ComponentStaticMesh, SourceMaterials, ComponentTransform, GeometryCollection, true);
			
			RemoveAndDestroyComponent(OldComponent);
			
			GeometryCollectionPiece.InstancerOutput->OutputComponent = nullptr;
	
			// Sets the GeometryIndex, to identify which this piece is when dealing with the geometry collection
			int32 GeometryIndex = GeometryCollection->NumElements(FGeometryCollection::TransformGroup) - 1;
			GeometryCollectionPiece.GeometryIndex = GeometryIndex;
		}
		
		GeometryCollection->InitializeMaterials();
	
		// Adds a singular root node to all of the meshes. This automatically makes Level 0 = 1, and Level 1 = the rest of your meshs.
		AddSingleRootNodeIfRequired(GeometryCollection);

		for (auto & Cluster : Clusters)
		{
			int32 FractureIndex = Cluster.Key.Key;
			int32 ClusterIndex = Cluster.Key.Value;

			if (FractureIndex <= 1)
			{
				// If level is 1, then no need to bump up the levels
				continue;
			}

			// Getting the minimum insertion index to figure out the parent/transform of the new bone.
			int32 MinInsertionPoint = INT_MAX; 
			
			TArray<int32> BoneIndices;
			BoneIndices.Empty();
			for (auto & Piece : Cluster.Value)
			{
				if (Piece == nullptr) continue;
				
				BoneIndices.Add(Piece->GeometryIndex);
				MinInsertionPoint = std::min(MinInsertionPoint, Piece->GeometryIndex);
			}

			
			for (int32 i = 0; i < FractureIndex - 1; i++)
			{
				TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollection->GetGeometryCollection();
				if (FGeometryCollection* GeometryCollectionObj = GeometryCollectionPtr.Get())
				{
					FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNode(GeometryCollectionObj, MinInsertionPoint, BoneIndices, false);
				}
			}
		}
		
		// One-of detail attributes, so only need to take the first GeometryCollectionPiece
		FHoudiniGeometryCollectionPiece& FirstPiece = GeometryCollectionPieces[0];
	
		// Apply all geometry collection attributes that are static across all pieces.
		ApplyGeometryCollectionAttributes(GeometryCollection, FirstPiece);
	
		// Set output object
		OutputObject.OutputObject = GeometryCollectionActor;
		OutputObject.OutputComponent = GeometryCollectionComponent;
		
		// Mark the render state dirty otherwise it won't appear until you move it
	        GeometryCollectionComponent->MarkRenderStateDirty();
	}
}

UGeometryCollectionComponent*
FHoudiniGeometryCollectionTranslator::CreateGeometryCollectionComponent(UObject *InOuterComponent)
{
	// Create a new SMC as we couldn't find an existing one
	USceneComponent* OuterSceneComponent = Cast<USceneComponent>(InOuterComponent);
	UObject * Outer = nullptr;
	if (IsValid(OuterSceneComponent))
		Outer = OuterSceneComponent->GetOwner() ? OuterSceneComponent->GetOwner() : OuterSceneComponent->GetOuter();

	UGeometryCollectionComponent *GeometryCollectionComponent = NewObject<UGeometryCollectionComponent>(Outer, UGeometryCollectionComponent::StaticClass(), NAME_None, RF_Transactional);

	// Initialize it
	GeometryCollectionComponent->SetVisibility(true);

	// Change the creation method so the component is listed in the details panels
	GeometryCollectionComponent->CreationMethod = EComponentCreationMethod::Instance;

	// Attach created static mesh component to our Houdini component.
	GeometryCollectionComponent->AttachToComponent(OuterSceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
	GeometryCollectionComponent->OnComponentCreated();
	GeometryCollectionComponent->RegisterComponent();

	bool HasDebugDrawComponent = false;
	if (OuterSceneComponent->GetOwner()->FindComponentByClass(UGeometryCollectionDebugDrawComponent::StaticClass()))
	{
		HasDebugDrawComponent = true;
	}
		

	if (!HasDebugDrawComponent)
	{
		// AAAA
		UGeometryCollectionDebugDrawComponent * GeometryCollectionDrawComponent = NewObject<UGeometryCollectionDebugDrawComponent>(Outer, UGeometryCollectionDebugDrawComponent::StaticClass(), NAME_None, RF_Transactional);

		GeometryCollectionDrawComponent->CreationMethod = EComponentCreationMethod::Instance;
		GeometryCollectionDrawComponent->RegisterComponent();
		GeometryCollectionDrawComponent->OnComponentCreated();
	}
	
	return GeometryCollectionComponent;
}



bool
FHoudiniGeometryCollectionTranslator::RemoveAndDestroyComponent(UObject* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent);
	if (IsValid(SceneComponent))
	{
		if (SceneComponent->IsA(UGeometryCollectionComponent::StaticClass()))
		{
			UActorComponent * DebugDrawComponent = SceneComponent->GetOwner()->FindComponentByClass(UGeometryCollectionDebugDrawComponent::StaticClass());
			if (DebugDrawComponent)
			{
				RemoveAndDestroyComponent(DebugDrawComponent);
			}
		}
		// Remove from the HoudiniAssetActor
		if (SceneComponent->GetOwner())
			SceneComponent->GetOwner()->RemoveOwnedComponent(SceneComponent);

		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SceneComponent->UnregisterComponent();
		SceneComponent->DestroyComponent();

		return true;
	}

	return false;
}

bool FHoudiniGeometryCollectionTranslator::GetGeometryCollectionData(const TArray<UHoudiniOutput*>& InAllOutputs, const FHoudiniPackageParams& InPackageParams,
	TMap<FString, FHoudiniGeometryCollectionData>& OutGeometryCollectionData)
{
	for (auto & HoudiniOutput : InAllOutputs)
	{
		if (!IsGeometryCollectionInstancer(HoudiniOutput))
		{
			continue;
		}

		FHoudiniGeometryCollectionPiece NewPiece;

		for (auto & Pair : HoudiniOutput->OutputObjects)
		{
			NewPiece.InstancerOutputIdentifier = &(Pair.Key);
			NewPiece.InstancerOutput = &(Pair.Value);

			int GeoId = NewPiece.InstancerOutputIdentifier->GeoId;
			int PartId = NewPiece.InstancerOutputIdentifier->PartId;

			// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
			int NumInstancedParts = 1;

			TArray<HAPI_PartId> InstancedPartIds;
			InstancedPartIds.SetNumZeroed(NumInstancedParts);
			if (FHoudiniApi::GetInstancedPartIds(
				FHoudiniEngine::Get().GetSession(), GeoId, PartId,
				InstancedPartIds.GetData(), 0, NumInstancedParts) != HAPI_RESULT_SUCCESS)
			{
				return false;
			}

			if (InstancedPartIds.Num() > 0)
			{
				NewPiece.InstancedPartId = InstancedPartIds[0];
			}
		}

		GetFracturePieceAttribute(NewPiece.InstancerOutputIdentifier->GeoId, NewPiece.InstancedPartId, NewPiece.FractureIndex);
		GetClusterPieceAttribute(NewPiece.InstancerOutputIdentifier->GeoId, NewPiece.InstancedPartId, NewPiece.ClusterIndex);
		GetGeometryCollectionNameAttribute(NewPiece.InstancerOutputIdentifier->GeoId, NewPiece.InstancedPartId, NewPiece.GeometryCollectionName);

		FHoudiniGeometryCollectionData * GeometryCollectionData = nullptr;

		if (!OutGeometryCollectionData.Contains(NewPiece.GeometryCollectionName))
		{
			FHoudiniGeometryCollectionData & NewData = OutGeometryCollectionData.Add(NewPiece.GeometryCollectionName, FHoudiniGeometryCollectionData(*NewPiece.InstancerOutputIdentifier, InPackageParams));
			GeometryCollectionData = &NewData;
		}
		else
		{
			GeometryCollectionData = &OutGeometryCollectionData[NewPiece.GeometryCollectionName];
		}

		// Add _GM suffix to the split str, to distinguish GeometryCollections from StaticMeshes
		// Additionally add the name to distinguish it from other GC outputs
		FString SplitStrName = TEXT("");
		if (!NewPiece.GeometryCollectionName.IsEmpty())
		{
			SplitStrName += "_" + NewPiece.GeometryCollectionName;
		}
		GeometryCollectionData->PackParams.SplitStr = SplitStrName + "_GC";

		GeometryCollectionData->Identifier.SplitIdentifier = GeometryCollectionData->PackParams.SplitStr;
		
		GeometryCollectionData->GeometryCollectionPieces.Add(NewPiece);
	}

	return OutGeometryCollectionData.Num() > 0;
}


AGeometryCollectionActor* FHoudiniGeometryCollectionTranslator::CreateNewGeometryActor(UWorld * InWorld, const FString & InActorName, const FTransform InTransform)
{
	// Create the new Geometry Collection actor
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(InWorld->GetCurrentLevel(), AGeometryCollectionActor::StaticClass(), FName(InActorName));
	// SpawnParameters.Name = FName(InActorName);
	
	AGeometryCollectionActor* NewActor = InWorld->SpawnActor<AGeometryCollectionActor>(SpawnParameters);
	check(NewActor->GetGeometryCollectionComponent());

	// Set the Geometry Collection asset in the new actor

	// copy transform of original static mesh actor to this new actor
	NewActor->SetActorLabel(InActorName);
	NewActor->SetActorTransform(InTransform);
	
	return NewActor;
}

bool FHoudiniGeometryCollectionTranslator::GetGeometryCollectionNames(TArray<UHoudiniOutput*>& InAllOutputs,
	TSet<FString>& Names)
{
	for (auto & HoudiniOutput : InAllOutputs)
	{
		if (!IsGeometryCollectionInstancer(HoudiniOutput))
		{
			continue;
		}

		int32 InstancedPartId = -1;

		int GeoId = -1;
		int PartId = -1;
		
		for (auto & Pair : HoudiniOutput->OutputObjects)
		{
			GeoId = Pair.Key.GeoId;
			PartId = Pair.Key.PartId;

			// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
			int NumInstancedParts = 1;

			TArray<HAPI_PartId> InstancedPartIds;
			InstancedPartIds.SetNumZeroed(NumInstancedParts);
			if (FHoudiniApi::GetInstancedPartIds(
				FHoudiniEngine::Get().GetSession(), GeoId, PartId,
				InstancedPartIds.GetData(), 0, NumInstancedParts) != HAPI_RESULT_SUCCESS)
			{
				return false;
			}

			if (InstancedPartIds.Num() > 0)
			{
				InstancedPartId = InstancedPartIds[0];
			}
		}

		FString GCName;
		GetGeometryCollectionNameAttribute(GeoId, InstancedPartId, GCName);

		Names.Add(GCName);
	}

	return true;
}

bool FHoudiniGeometryCollectionTranslator::GetFracturePieceAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, int& OutInt)
{
	bool HasFractureAttribute = false;
	HAPI_AttributeInfo AttriInfo;
	FHoudiniApi::AttributeInfo_Init(&AttriInfo);
	TArray<int> IntData;
	IntData.Empty();

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
	HAPI_UNREAL_ATTRIB_GC_PIECE, AttriInfo, IntData, 1))
	{
		if (IntData.Num() > 0)
		{
			HasFractureAttribute = true;
			OutInt = IntData[0];
		}
			
	}

	return HasFractureAttribute;
}

bool FHoudiniGeometryCollectionTranslator::GetClusterPieceAttribute(const HAPI_NodeId& GeoId, const HAPI_NodeId& PartId, int& OutInt)
{
	bool HasClusterAttribute = false;
	HAPI_AttributeInfo AttriInfo;
	FHoudiniApi::AttributeInfo_Init(&AttriInfo);
	TArray<int> IntData;
	IntData.Empty();

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
	HAPI_UNREAL_ATTRIB_GC_CLUSTER_PIECE, AttriInfo, IntData, 1))
	{
		if (IntData.Num() > 0)
		{
			HasClusterAttribute = true;
			OutInt = IntData[0];
		}
			
	}

	if (!HasClusterAttribute) OutInt = -1; // Cluster piece can be null
	
	return HasClusterAttribute;
}

bool FHoudiniGeometryCollectionTranslator::GetGeometryCollectionNameAttribute(const HAPI_NodeId& GeoId,
	const HAPI_NodeId& PartId, FString& OutName)
{
	bool HasAttribute = false;
	HAPI_AttributeInfo AttriInfo;
	FHoudiniApi::AttributeInfo_Init(&AttriInfo);
	TArray<FString> StrData;
	StrData.Empty();

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(GeoId, PartId,
	HAPI_UNREAL_ATTRIB_GC_NAME, AttriInfo, StrData, 1))
	{
		if (StrData.Num() > 0)
		{
			HasAttribute = true;
			OutName = StrData[0];
		}
			
	}

	if (!HasAttribute)
	{
		OutName = TEXT("");
	}

	return HasAttribute;
}

bool FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancer(const UHoudiniOutput* HoudiniOutput)
{
	if (HoudiniOutput == nullptr)
	{
		return false;
	}
	
	EHoudiniOutputType OutputType = HoudiniOutput->Type;
	if (OutputType != EHoudiniOutputType::Instancer)
	{
		return false;
	}

	// Check geoparts type so we don't have to get a Houdini session to call this function if not necessary
	for (auto & GeoPart : HoudiniOutput->GetHoudiniGeoPartObjects())
	{
		if (GeoPart.InstancerType == EHoudiniInstancerType::GeometryCollection)
		{
			return true;
		}
	}

	int InstancerGeoId = -1;
	int InstancerPartId = -1;

	for (auto & Pair : HoudiniOutput->OutputObjects)
	{
		InstancerGeoId = Pair.Key.GeoId;
		InstancerPartId = Pair.Key.PartId;
	}

	if (InstancerGeoId == -1 || InstancerPartId == -1)
	{
		return false;
	}

	if (!IsGeometryCollectionInstancerPart(InstancerGeoId, InstancerPartId))
	{
		return false;
	}

	return true;
}

bool FHoudiniGeometryCollectionTranslator::IsGeometryCollectionMesh(const UHoudiniOutput* HoudiniOutput)
{
	EHoudiniOutputType OutputType = HoudiniOutput->Type;
	if (OutputType != EHoudiniOutputType::Mesh)
	{
		return false;
	}
	
	int32 GeoId = -1;
	int32 PartId = -1;

	for (auto & Pair : HoudiniOutput->OutputObjects)
	{
		GeoId = Pair.Key.GeoId;
		PartId = Pair.Key.PartId;

		if (Pair.Value.bIsGeometryCollectionPiece)
		{
			return true;
		}
	}

	if (GeoId == -1 || PartId == -1)
	{
		return false;
	}

	int FractureIndex = 0;

	if (!GetFracturePieceAttribute(GeoId, PartId, FractureIndex))
	{
		return false;
	}
	else if (FractureIndex <= 0)
	{
		return false;
	}
	
	return true;
}

bool FHoudiniGeometryCollectionTranslator::IsGeometryCollectionInstancerPart(const HAPI_NodeId InstancerGeoId, const HAPI_PartId InstancerPartId)
{
	HAPI_ParmId InstancedPartId = -1;

	// Assume that there is only one part per instance. This is always true for now but may need to be looked at later.
	int NumInstancedParts = 1;
	
	TArray<HAPI_PartId> InstancedPartIds;
	InstancedPartIds.SetNumZeroed(NumInstancedParts);
	if (FHoudiniApi::GetInstancedPartIds(
		FHoudiniEngine::Get().GetSession(), InstancerGeoId, InstancerPartId,
		InstancedPartIds.GetData(), 0, NumInstancedParts) != HAPI_RESULT_SUCCESS)
	{
		return false;
	}

	if (InstancedPartIds.Num() <= 0)
	{
		return false;
	}
	else
	{
		InstancedPartId = InstancedPartIds[0];
	}

	if (InstancedPartId == -1)
	{
		return false;
	}

	int FractureIndex = 0;

	if (!GetFracturePieceAttribute(InstancerGeoId, InstancedPartId, FractureIndex))
	{
		return false;
	}
	else if (FractureIndex <= 0)
	{
		return false;
	}

	return true;
}


void FHoudiniGeometryCollectionTranslator::ApplyGeometryCollectionAttributes(UGeometryCollection* GeometryCollection,
	FHoudiniGeometryCollectionPiece FirstPiece)
{
	int32 GeoId = FirstPiece.InstancerOutputIdentifier->GeoId;
	int32 PartId = FirstPiece.InstancedPartId;

	/*
	{
		// Damage thresholds are not yet available in vanilla 4.26. UNCOMMENT THIS IN FUTURE VERSIONS.
		// Clustering - Damage thresholds
		TArray<float> FloatData;
		FloatData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_CLUSTERING_DAMAGE_THRESHOLD;

		TArray<int> FloatDataSizes;
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

                if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeInfo(
                	FHoudiniEngine::Get().GetSession(), GeoId, PartId,
                	AttributeName, HAPI_AttributeOwner::HAPI_ATTROWNER_DETAIL, &AttributeInfo) && AttributeInfo.exists)
                {

                	FloatData.SetNumZeroed(AttributeInfo.totalArrayElements);
                	FloatDataSizes.SetNumZeroed(AttributeInfo.totalArrayElements);

                	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAttributeFloatArrayData(
                		FHoudiniEngine::Get().GetSession(), GeoId, PartId,
                		AttributeName, &AttributeInfo, (float*)FloatData.GetData(), AttributeInfo.totalArrayElements, FloatDataSizes.GetData(), 0, AttributeInfo.count))
                	{
                		if (FloatData.Num() > 0)
                		{
                			GeometryCollection->DamageThreshold = FloatData;
                		}
                	}
                }
	}
	*/

	{
		// Collisions - Collision Type
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_TYPE;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				int32 Result = IntData[0];
				if (Result < (int32)ECollisionTypeEnum::Chaos_Max)
				{
					ECollisionTypeEnum CollisionType = (ECollisionTypeEnum)Result;
					GeometryCollection->CollisionType = CollisionType;
				}
			}
		}
	}

	{
		// Collisions - Implicit Type
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_IMPLICIT_TYPE;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				int32 Result = IntData[0];
				// 0 = None, 1 = Box, 2 = Sphere, 3 = Capsule, 4 = Level Set
				EImplicitTypeEnum ImplicitType = EImplicitTypeEnum::Chaos_Implicit_None;
				if (Result != 0 && Result < (int32)EImplicitTypeEnum::Chaos_Max)
				{
					ImplicitType = (EImplicitTypeEnum)(Result - 1);
				}
				
				GeometryCollection->ImplicitType = ImplicitType;
			}
		}
	}


	{
		// Collisions - Min Level Set Resolution
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MIN_LEVEL_SET_RESOLUTION;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				GeometryCollection->MinLevelSetResolution = IntData[0];
			}
		}
	}

	{
		// Collisions - Max Level Set Resolution
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAX_LEVEL_SET_RESOLUTION;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				GeometryCollection->MaxLevelSetResolution = IntData[0];
			}
		}
	}

	{
		// Collisions - Min cluster level set resolution
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MIN_CLUSTER_LEVEL_SET_RESOLUTION;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				GeometryCollection->MinClusterLevelSetResolution = IntData[0];
			}
		}
	}
	
	{
		// Collisions - Max cluster level set resolution
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAX_CLUSTER_LEVEL_SET_RESOLUTION;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				GeometryCollection->MaxClusterLevelSetResolution = IntData[0];
			}
		}
	}
	
	{
		// Collisions - Object reduction percentage
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<float> FloatData;
		FloatData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_OBJECT_REDUCTION_PERCENTAGE;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId,
                AttributeName, AttriInfo, FloatData, 1))
		{
			if (FloatData.Num() > 0)
			{
				GeometryCollection->CollisionObjectReductionPercentage = FloatData[0];
			}
		}
	}

	{
		// Collisions - Mass as density
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MASS_AS_DENSITY;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				bool Result = (IntData[0] == 1);
				GeometryCollection->bMassAsDensity = Result;
			}
		}
	}

	{
		// Collisions - Mass
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<float> FloatData;
		FloatData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MASS;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId,
                AttributeName, AttriInfo, FloatData, 1))
		{
			if (FloatData.Num() > 0)
			{
				GeometryCollection->Mass = FloatData[0];
			}
		}
	}

	{
		// Collisions - Minimum Mass Clamp
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<float> FloatData;
		FloatData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MINIMUM_MASS_CLAMP;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId,
                AttributeName, AttriInfo, FloatData, 1))
		{
			if (FloatData.Num() > 0)
			{
				GeometryCollection->MinimumMassClamp = FloatData[0];
			}
		}
	}
	
	{
		// Collisions - Collision particles fraction
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<float> FloatData;
		FloatData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_COLLISION_PARTICLES_FRACTION;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(GeoId, PartId,
                AttributeName, AttriInfo, FloatData, 1))
		{
			if (FloatData.Num() > 0)
			{
				GeometryCollection->CollisionParticlesFraction = FloatData[0];
			}
		}
	}
	
	{
		// Collisions - Maximum collision particles
		HAPI_AttributeInfo AttriInfo;
		FHoudiniApi::AttributeInfo_Init(&AttriInfo);
		TArray<int32> IntData;
		IntData.Empty();

		const char * AttributeName = HAPI_UNREAL_ATTRIB_GC_COLLISIONS_MAXIMUM_COLLISION_PARTICLES;

		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
		
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(GeoId, PartId,
                AttributeName, AttriInfo, IntData, 1))
		{
			if (IntData.Num() > 0)
			{
				GeometryCollection->MaximumCollisionParticles = IntData[0];
			}
		}
	}
}


void FHoudiniGeometryCollectionTranslator::AppendStaticMesh(
	const UStaticMesh* StaticMesh,
	const TArray<UMaterialInterface*>& Materials,
	const FTransform& StaticMeshTransform,
	UGeometryCollection* GeometryCollectionObject,
	const bool& ReindexMaterials,
	const int32& Level)
{
	if (!IsValid(StaticMesh))
	{
		return;
	}

	check(GeometryCollectionObject);
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get();
	check(GeometryCollection);

	// @todo : Discuss how to handle multiple LOD's
	if (StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
	{
		FStaticMeshVertexBuffers& VertexBuffer = StaticMesh->RenderData->LODResources[0].VertexBuffers;

		// vertex information
		TManagedArray<FVector>& Vertex = GeometryCollection->Vertex;
		TManagedArray<FVector>& TangentU = GeometryCollection->TangentU;
		TManagedArray<FVector>& TangentV = GeometryCollection->TangentV;
		TManagedArray<FVector>& Normal = GeometryCollection->Normal;
		TManagedArray<FVector2D>& UV = GeometryCollection->UV;
		TManagedArray<FLinearColor>& Color = GeometryCollection->Color;
		TManagedArray<int32>& BoneMap = GeometryCollection->BoneMap;
		TManagedArray<FLinearColor>& BoneColor = GeometryCollection->BoneColor;
		TManagedArray<FString>& BoneName = GeometryCollection->BoneName;

		const int32 VertexCount = VertexBuffer.PositionVertexBuffer.GetNumVertices();
		int InitialNumVertices = GeometryCollection->NumElements(FGeometryCollection::VerticesGroup);
		int VertexStart = GeometryCollection->AddElements(VertexCount, FGeometryCollection::VerticesGroup);
		FVector Scale = StaticMeshTransform.GetScale3D();
		for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			int VertexOffset = VertexStart + VertexIndex;
			Vertex[VertexOffset] = VertexBuffer.PositionVertexBuffer.VertexPosition(VertexIndex) * Scale;
			BoneMap[VertexOffset] = GeometryCollection->NumElements(FGeometryCollection::TransformGroup);

			TangentU[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.VertexTangentX(VertexIndex);
			TangentV[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.VertexTangentY(VertexIndex);
			Normal[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex);

			// @todo : Support multiple UV's per vertex based on MAX_STATIC_TEXCOORDS
			UV[VertexOffset] = VertexBuffer.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, 0);
			if (VertexBuffer.ColorVertexBuffer.GetNumVertices() == VertexCount)
				Color[VertexOffset] = VertexBuffer.ColorVertexBuffer.VertexColor(VertexIndex);
			// Houdini plugin: Vertex colors seem to not be added properly for Houdini generated materials with textures. 
			// Hack it by setting it to white for now. TODO: Revisit this.
			else if (VertexOffset < Color.Num())
				Color[VertexOffset] = FLinearColor(1, 1, 1, 1);
		}

		// Triangle Indices
		TManagedArray<FIntVector>& Indices = GeometryCollection->Indices;
		TManagedArray<bool>& Visible = GeometryCollection->Visible;
		TManagedArray<int32>& MaterialID = GeometryCollection->MaterialID;
		TManagedArray<int32>& MaterialIndex = GeometryCollection->MaterialIndex;

		FRawStaticIndexBuffer& IndexBuffer = StaticMesh->RenderData->LODResources[0].IndexBuffer;
		FIndexArrayView IndexBufferView = IndexBuffer.GetArrayView();
		const int32 IndicesCount = IndexBuffer.GetNumIndices() / 3;
		int InitialNumIndices = GeometryCollection->NumElements(FGeometryCollection::FacesGroup);
		int IndicesStart = GeometryCollection->AddElements(IndicesCount, FGeometryCollection::FacesGroup);
		for (int32 IndicesIndex = 0, StaticIndex = 0; IndicesIndex < IndicesCount; IndicesIndex++, StaticIndex += 3)
		{
			int32 IndicesOffset = IndicesStart + IndicesIndex;
			Indices[IndicesOffset] = FIntVector(
				IndexBufferView[StaticIndex] + VertexStart,
				IndexBufferView[StaticIndex + 1] + VertexStart,
				IndexBufferView[StaticIndex + 2] + VertexStart);
			Visible[IndicesOffset] = true;
			MaterialID[IndicesOffset] = 0;
			MaterialIndex[IndicesOffset] = IndicesOffset;
		}

		// Geometry transform
		TManagedArray<FTransform>& Transform = GeometryCollection->Transform;

		int32 TransformIndex1 = GeometryCollection->AddElements(1, FGeometryCollection::TransformGroup);
		Transform[TransformIndex1] = StaticMeshTransform;
		Transform[TransformIndex1].SetScale3D(FVector(1.f, 1.f, 1.f));

		// Bone Hierarchy - Added at root with no common parent
		TManagedArray<int32>& Parent = GeometryCollection->Parent;
		TManagedArray<int32>& SimulationType = GeometryCollection->SimulationType;
		Parent[TransformIndex1] = FGeometryCollection::Invalid;
		SimulationType[TransformIndex1] = FGeometryCollection::ESimulationTypes::FST_Rigid;

		const FColor RandBoneColor(FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, FMath::Rand() % 100 + 5, 255);
		BoneColor[TransformIndex1] = FLinearColor(RandBoneColor);
		BoneName[TransformIndex1] = StaticMesh->GetName();

		// GeometryGroup
		int GeometryIndex = GeometryCollection->AddElements(1, FGeometryCollection::GeometryGroup);

		TManagedArray<int32>& TransformIndex = GeometryCollection->TransformIndex;
		TManagedArray<FBox>& BoundingBox = GeometryCollection->BoundingBox;
		TManagedArray<float>& InnerRadius = GeometryCollection->InnerRadius;
		TManagedArray<float>& OuterRadius = GeometryCollection->OuterRadius;
		TManagedArray<int32>& VertexStartArray = GeometryCollection->VertexStart;
		TManagedArray<int32>& VertexCountArray = GeometryCollection->VertexCount;
		TManagedArray<int32>& FaceStartArray = GeometryCollection->FaceStart;
		TManagedArray<int32>& FaceCountArray = GeometryCollection->FaceCount;

		TransformIndex[GeometryIndex] = BoneMap[VertexStart];
		VertexStartArray[GeometryIndex] = InitialNumVertices;
		VertexCountArray[GeometryIndex] = VertexCount;
		FaceStartArray[GeometryIndex] = InitialNumIndices;
		FaceCountArray[GeometryIndex] = IndicesCount;

		// TransformGroup
		TManagedArray<int32>& TransformToGeometryIndexArray = GeometryCollection->TransformToGeometryIndex;
		TransformToGeometryIndexArray[TransformIndex1] = GeometryIndex;

		FVector Center(0);
		for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart + VertexCount; VertexIndex++)
		{
			Center += Vertex[VertexIndex];
		}
		if (VertexCount) Center /= VertexCount;

		// Inner/Outer edges, bounding box
		BoundingBox[GeometryIndex] = FBox(ForceInitToZero);
		InnerRadius[GeometryIndex] = FLT_MAX;
		OuterRadius[GeometryIndex] = -FLT_MAX;
		for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart + VertexCount; VertexIndex++)
		{
			BoundingBox[GeometryIndex] += Vertex[VertexIndex];

			float Delta = (Center - Vertex[VertexIndex]).Size();
			InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
			OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
		}

		// Inner/Outer centroid
		for (int fdx = IndicesStart; fdx < IndicesStart + IndicesCount; fdx++)
		{
			FVector Centroid(0);
			for (int e = 0; e < 3; e++)
			{
				Centroid += Vertex[Indices[fdx][e]];
			}
			Centroid /= 3;

			float Delta = (Center - Centroid).Size();
			InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
			OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
		}

		// Inner/Outer edges
		for (int fdx = IndicesStart; fdx < IndicesStart + IndicesCount; fdx++)
		{
			for (int e = 0; e < 3; e++)
			{
				int i = e, j = (e + 1) % 3;
				FVector Edge = Vertex[Indices[fdx][i]] + 0.5 * (Vertex[Indices[fdx][j]] - Vertex[Indices[fdx][i]]);
				float Delta = (Center - Edge).Size();
				InnerRadius[GeometryIndex] = FMath::Min(InnerRadius[GeometryIndex], Delta);
				OuterRadius[GeometryIndex] = FMath::Max(OuterRadius[GeometryIndex], Delta);
			}
		}

		// for each material, add a reference in our GeometryCollectionObject
		const int32 MaterialStart = GeometryCollectionObject->Materials.Num();
		const int32 NumMeshMaterials = Materials.Num();
		GeometryCollectionObject->Materials.Reserve(MaterialStart + NumMeshMaterials);

		for (int32 Index = 0; Index < NumMeshMaterials; ++Index)
		{
			UMaterialInterface* CurrMaterial = Materials[Index];

			// Possible we have a null entry - replace with default
			if (CurrMaterial == nullptr)
			{
				CurrMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			GeometryCollectionObject->Materials.Add(CurrMaterial);
		}

		TManagedArray<FGeometryCollectionSection>& Sections = GeometryCollection->Sections;

		// We make sections that mirror what is in the static mesh.  Note that this isn't explicitly
		// necessary since we reindex after all the meshes are added, but it is a good step to have
		// optimal min/max vertex index right from the static mesh.  All we really need to do is
		// assign material ids and rely on reindexing, in theory
		for (const FStaticMeshSection& CurrSection : StaticMesh->RenderData->LODResources[0].Sections)
		{
			// create new section
			int32 SectionIndex = GeometryCollection->AddElements(1, FGeometryCollection::MaterialGroup);

			Sections[SectionIndex].MaterialID = MaterialStart + CurrSection.MaterialIndex;

			Sections[SectionIndex].FirstIndex = IndicesStart * 3 + CurrSection.FirstIndex;
			Sections[SectionIndex].MinVertexIndex = VertexStart + CurrSection.MinVertexIndex;

			Sections[SectionIndex].NumTriangles = CurrSection.NumTriangles;
			Sections[SectionIndex].MaxVertexIndex = VertexStart + CurrSection.MaxVertexIndex;

			// set the MaterialID for all of the faces
			// note the divide by 3 - the GeometryCollection stores indices in tuples of 3 rather than in a flat array
			for (int32 i = Sections[SectionIndex].FirstIndex / 3; i < Sections[SectionIndex].FirstIndex / 3 + Sections[SectionIndex].NumTriangles; ++i)
			{
				MaterialID[i] = Sections[SectionIndex].MaterialID;
			}
		}

		if (ReindexMaterials)
		{
			GeometryCollection->ReindexMaterials();
		}
	}

}

// Copied from FractureToolEmbed.h
void FHoudiniGeometryCollectionTranslator::AddSingleRootNodeIfRequired(UGeometryCollection* GeometryCollectionObject)
{
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(GeometryCollection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(GeometryCollection);
		}
	}
}