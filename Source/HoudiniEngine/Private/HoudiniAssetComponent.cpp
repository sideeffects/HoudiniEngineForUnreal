// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HAPI.h"
#include <string>

/** Vertex Buffer */
class FHoudiniMeshVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI()
	{
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex),NULL,BUF_Static);

		// Copy the vertex data into the vertex buffer.
		void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI,0,Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData,Vertices.GetTypedData(),Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}

};

/** Index Buffer */
class FHoudiniMeshIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI()
	{
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32),Indices.Num() * sizeof(int32),NULL,BUF_Static);

		// Write the indices to the index buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Indices.Num() * sizeof(int32),RLM_WriteOnly);
		FMemory::Memcpy(Buffer,Indices.GetTypedData(),Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};

/** Vertex Factory */
class FHoudiniMeshVertexFactory : public FLocalVertexFactory
{
public:

	FHoudiniMeshVertexFactory()
	{}


	/** Initialization */
	void Init(const FHoudiniMeshVertexBuffer* VertexBuffer)
	{
		check(!IsInRenderingThread());

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitHoudiniMeshVertexFactory,
			FHoudiniMeshVertexFactory*,VertexFactory,this,
			const FHoudiniMeshVertexBuffer*,VertexBuffer,VertexBuffer,
		{
			// Initialize the vertex factory's stream components.
			DataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			NewData.TextureCoordinates.Add(
				FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
				);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			VertexFactory->SetData(NewData);
		});
	}
};

/** Scene proxy */
class FHoudiniMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FHoudiniMeshSceneProxy(UHoudiniAssetComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance())
	{
		const FColor VertexColor(255,255,255);

		// Add each triangle to the vertex/index buffer
		for(int TriIdx=0; TriIdx<Component->HoudiniMeshTris.Num(); TriIdx++)
		{
			FHoudiniMeshTriangle& Tri = Component->HoudiniMeshTris[TriIdx];

			const FVector Edge01 = (Tri.Vertex1 - Tri.Vertex0);
			const FVector Edge02 = (Tri.Vertex2 - Tri.Vertex0);

			const FVector TangentX = Edge01.SafeNormal();
			const FVector TangentZ = (Edge02 ^ Edge01).SafeNormal();
			const FVector TangentY = (TangentX ^ TangentZ).SafeNormal();

			FDynamicMeshVertex Vert0;
			Vert0.Position = Tri.Vertex0;
			Vert0.Color = VertexColor;
			Vert0.SetTangents(TangentX, TangentY, TangentZ);
			int32 VIndex = VertexBuffer.Vertices.Add(Vert0);
			IndexBuffer.Indices.Add(VIndex);

			FDynamicMeshVertex Vert1;
			Vert1.Position = Tri.Vertex1;
			Vert1.Color = VertexColor;
			Vert1.SetTangents(TangentX, TangentY, TangentZ);
			VIndex = VertexBuffer.Vertices.Add(Vert1);
			IndexBuffer.Indices.Add(VIndex);

			FDynamicMeshVertex Vert2;
			Vert2.Position = Tri.Vertex2;
			Vert2.Color = VertexColor;
			Vert2.SetTangents(TangentX, TangentY, TangentZ);
			VIndex = VertexBuffer.Vertices.Add(Vert2);
			IndexBuffer.Indices.Add(VIndex);
		}

		// Init vertex factory
		VertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FHoudiniMeshSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View)
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_HoudiniMeshSceneProxy_DrawDynamicElements );

		const bool bWireframe = View->Family->EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy WireframeMaterialInstance(
			GEngine->WireframeMaterial->GetRenderProxy(IsSelected()),
			FLinearColor(0, 0.5f, 1.f)
			);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = &WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy(IsSelected());
		}

		// Draw the mesh.
		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = &VertexFactory;
		Mesh.MaterialRenderProxy = MaterialProxy;
		BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true);
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		PDI->DrawMesh(Mesh);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const OVERRIDE
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:

	UMaterialInterface* Material;
	FHoudiniMeshVertexBuffer VertexBuffer;
	FHoudiniMeshIndexBuffer IndexBuffer;
	FHoudiniMeshVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UHoudiniAssetComponent::UHoudiniAssetComponent( const FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
	, AssetId( -1 )
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	AssetLibraryPath = "Blah";
}

bool UHoudiniAssetComponent::InstantiateAssetFromPath( const FString& Path )
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, Path);
		GEngine->AddOnScreenDebugMessage( -1, 5.f, FColor::Yellow, FString::FromInt( AssetId ) );
	}

	if ( AssetId >= 0 )
		return true;

	FString escapedPath = Path.ReplaceCharWithEscapedChar();
	std::wstring wpath( *escapedPath );
	std::string path( wpath.begin( ), wpath.end( ) );

	HAPI_AssetLibraryId library_id;
	HAPI_Result result = HAPI_LoadAssetLibraryFromFile( path.c_str(), &library_id );
	if ( result )
		return false;

	int asset_count;
	result = HAPI_GetAvailableAssetCount( library_id, &asset_count );
	if ( result )
		return false;

	int* asset_names_sh = new int[ asset_count ];
	result = HAPI_GetAvailableAssets( library_id, asset_names_sh, asset_count );
	if ( result )
		return false;

	int first_asset_name_length;
	result = HAPI_GetStringBufLength( asset_names_sh[ 0 ], &first_asset_name_length );
	if ( result )
		return false;

	char* first_asset_name = new char[ first_asset_name_length ];
	result = HAPI_GetString( asset_names_sh[ 0 ], first_asset_name, first_asset_name_length );
	if ( result )
		return false;

	result = HAPI_InstantiateAsset( first_asset_name, true, &AssetId );
	if ( result )
		return false;

	HAPI_AssetInfo asset_info;
	result = HAPI_GetAssetInfo( AssetId, &asset_info );
	if ( result )
		return false;

	HAPI_PartInfo part_info;
	result = HAPI_GetPartInfo( AssetId, 0, 0, 0, &part_info );
	if ( result )
		return false;

	int* vertex_list = new int[ part_info.vertexCount ];
	result = HAPI_GetVertexList( AssetId, 0, 0, 0, vertex_list, 0, part_info.vertexCount );
	if ( result )
		return false;

	HAPI_AttributeInfo attrib_info;
	result = HAPI_GetAttributeInfo( AssetId, 0, 0, 0, "P", HAPI_ATTROWNER_POINT, &attrib_info );
	if ( result )
		return false;

	float* positions = new float[ attrib_info.count * attrib_info.tupleSize ];
	result = HAPI_GetAttributeFloatData(
		AssetId, 0, 0, 0, "P", &attrib_info, positions, 0, attrib_info.count );
	if ( result )
		return false;

	HoudiniMeshTris.Reset();
	for ( int i = 0; i < part_info.vertexCount / 3; ++i )
	{
		FHoudiniMeshTriangle triangle;

		triangle.Vertex0.X = positions[ vertex_list[ i * 3 + 0 ] * 3 + 0 ] * 100.0f;
		triangle.Vertex0.Y = positions[ vertex_list[ i * 3 + 0 ] * 3 + 1 ] * 100.0f;
		triangle.Vertex0.Z = positions[ vertex_list[ i * 3 + 0 ] * 3 + 2 ] * 100.0f;

		triangle.Vertex1.X = positions[ vertex_list[ i * 3 + 1 ] * 3 + 0 ] * 100.0f;
		triangle.Vertex1.Y = positions[ vertex_list[ i * 3 + 1 ] * 3 + 1 ] * 100.0f;
		triangle.Vertex1.Z = positions[ vertex_list[ i * 3 + 1 ] * 3 + 2 ] * 100.0f;

		triangle.Vertex2.X = positions[ vertex_list[ i * 3 + 2 ] * 3 + 0 ] * 100.0f;
		triangle.Vertex2.Y = positions[ vertex_list[ i * 3 + 2 ] * 3 + 1 ] * 100.0f;
		triangle.Vertex2.Z = positions[ vertex_list[ i * 3 + 2 ] * 3 + 2 ] * 100.0f;

		HoudiniMeshTris.Push( triangle );
	}

	if ( GEngine )
	{
		GEngine->AddOnScreenDebugMessage( -1, 5.f, FColor::Yellow, FString::FromInt( HoudiniMeshTris.Num() ) );
	}

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();

	return true;
}


FPrimitiveSceneProxy* UHoudiniAssetComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(HoudiniMeshTris.Num() > 0)
	{
		Proxy = new FHoudiniMeshSceneProxy(this);
	}
	return Proxy;
}

int32 UHoudiniAssetComponent::GetNumMaterials() const
{
	return 1;
}


FBoxSphereBounds UHoudiniAssetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX);
	NewBounds.SphereRadius = FMath::Sqrt(3.0f * FMath::Square(HALF_WORLD_MAX));
	return NewBounds;
}

