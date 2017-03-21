#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniApi.h"
#if 0
#if WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "FileCacheUtilities.h"
#include "StaticMeshResources.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineUtils.h"
#include "AssetRegistryModule.h"
#include "HoudiniCookHandler.h"
#include "HoudiniRuntimeSettings.h"

DEFINE_LOG_CATEGORY_STATIC( LogHoudiniTests, Log, All );

/** Test MatchExtensionString */
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeInstantiateAssetTest, "Houdini.Runtime.InstantiateAsset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter )

static float TestTickDelay = 1.0f;

struct FTestCookHandler : public FHoudiniCookParams, public IHoudiniCookHandler
{
    /** Transient cache of last baked parts */
    TMap<FHoudiniGeoPartObject, TWeakObjectPtr<class UPackage> > BakedStaticMeshPackagesForParts_;
    /** Transient cache of last baked materials and textures */
    TMap<FString, TWeakObjectPtr<class UPackage> > BakedMaterialPackagesForIds_;
    /** Cache of the temp cook content packages created by the asset for its materials/textures		    **/
    TMap<FHoudiniGeoPartObject, TWeakObjectPtr<class UPackage> > CookedTemporaryStaticMeshPackages_;
    /** Cache of the temp cook content packages created by the asset for its materials/textures		    **/
    TMap<FString, TWeakObjectPtr<class UPackage> > CookedTemporaryPackages_;

    FTestCookHandler( class UHoudiniAsset* InHoudiniAsset )
        : FHoudiniCookParams( InHoudiniAsset )
    {
        BakedStaticMeshPackagesForParts = &BakedStaticMeshPackagesForParts_;
        BakedMaterialPackagesForIds = &BakedMaterialPackagesForIds_;
        CookedTemporaryStaticMeshPackages = &CookedTemporaryStaticMeshPackages_;
        CookedTemporaryPackages = &CookedTemporaryPackages_;
    }

    virtual FString GetBakingBaseName( const struct FHoudiniGeoPartObject& GeoPartObject ) const override
    {
        if( GeoPartObject.HasCustomName() )
        {
            return GeoPartObject.PartName;
        }

        return FString::Printf( TEXT( "test_%d_%d_%d_%d" ),
            GeoPartObject.ObjectId, GeoPartObject.GeoId, GeoPartObject.PartId, GeoPartObject.SplitId );
    }


    virtual void SetStaticMeshGenerationParameters( class UStaticMesh* StaticMesh ) const override
    {
       
    }


    virtual class UMaterialInterface * GetAssignmentMaterial( const FString& MaterialName ) override
    {
        return nullptr;
    }


    virtual void ClearAssignmentMaterials() override
    {
        
    }


    virtual void AddAssignmentMaterial( const FString& MaterialName, class UMaterialInterface* MaterialInterface ) override
    {
        
    }


    virtual class UMaterialInterface * GetReplacementMaterial( const struct FHoudiniGeoPartObject& GeoPartObject, const FString& MaterialName ) override
    {
        return nullptr;
    }

};

struct FHVert
{
    int32 PointNum;
    float Nx, Ny, Nz;
    float uv0, uv1, uv2;
};
struct FSortVectors
{
    bool operator()( const FVector& A, const FVector& B ) const
    {
        if( A.X == B.X )
            if( A.Y == B.Y )
                if( A.Z == B.Z )
                    return false;
                else
                    return A.Z < B.Z;
            else
                return A.Y < B.Y;
        else
            return A.X < B.X;
    }
};

#if 0
struct FParamBlock
{
    
};

typedef TMap< FHoudiniGeoPartObject, UStaticMesh * > PartMeshMap;

struct IHoudiniPluginAPI 
{
    virtual void InstatiateLiveAsset( const char* AssetPath, TFunction<void ( HAPI_NodeId, FParamBlock ParamBlock )> OnComplete )
    {
        FFunctionGraphTask::CreateAndDispatchWhenReady( [=]() {
            OnComplete( -1, FParamBlock() );
        }
        , TStatId(), nullptr, ENamedThreads::GameThread );
    }

    virtual void CookLiveAsset( 
        HAPI_NodeId AssetId,
        const FParamBlock& ParamBlock, 
        PartMeshMap& CurrentParts,
        TFunction<void ( bool, PartMeshMap )> OnComplete )
    {
        FFunctionGraphTask::CreateAndDispatchWhenReady( [=]() {
            PartMeshMap NewParts;
            OnComplete( true, NewParts );
        }
        , TStatId(), nullptr, ENamedThreads::GameThread );
    }
};
#endif

bool FHoudiniEngineRuntimeInstantiateAssetTest::RunTest( const FString& Parameters )
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
    TArray<FAssetData> AssetData;
    AssetRegistryModule.Get().GetAssetsByPackageName( TEXT("/HoudiniEngine/TestBox"), AssetData );
    UHoudiniAsset* TestAsset = nullptr;
    if( AssetData.Num() > 0 )
    {
        TestAsset = Cast<UHoudiniAsset>( AssetData[ 0 ].GetAsset() );
    }

    HAPI_AssetLibraryId AssetLibraryId = -1;
    TArray< HAPI_StringHandle > AssetNames;
    HAPI_StringHandle AssetHapiName = -1;
    if( FHoudiniEngineUtils::GetAssetNames( TestAsset, AssetLibraryId, AssetNames ) )
    {
        AssetHapiName = AssetNames[ 0 ];
    }

    auto InstGUID = FGuid::NewGuid();
    FHoudiniEngineTask Task( EHoudiniEngineTaskType::AssetInstantiation, InstGUID );
    Task.Asset = TestAsset;
    Task.ActorName = TEXT( "TestActor" );
    Task.bLoadedComponent = false;
    Task.AssetLibraryId = AssetLibraryId;
    Task.AssetHapiName = AssetHapiName;
    FHoudiniEngine::Get().AddTask( Task );

    AddCommand( new FDelayedFunctionLatentCommand( [=] {
        // check back on status on Instantiate
        FHoudiniEngineTaskInfo InstantiateTaskInfo;
        if( FHoudiniEngine::Get().RetrieveTaskInfo( InstGUID, InstantiateTaskInfo ) )
        {
            if( InstantiateTaskInfo.TaskState != EHoudiniEngineTaskState::FinishedInstantiation )
            {
                AddError( FString::Printf( TEXT( "InstantiateTask.Result: %d" ), (int32)InstantiateTaskInfo.Result ) );
            }
            UE_LOG( LogHoudiniTests, Log, TEXT( "InstantiateTask.StatusText: %s" ), *InstantiateTaskInfo.StatusText.ToString() );
            
            FHoudiniEngine::Get().RemoveTaskInfo( InstGUID );

            // Cook asset
            auto CookGUID = FGuid::NewGuid();
            FHoudiniEngineTask CookTask( EHoudiniEngineTaskType::AssetCooking, CookGUID );
            CookTask.AssetId = InstantiateTaskInfo.AssetId;
            
            FHoudiniEngine::Get().AddTask( CookTask );

            AddCommand( new FDelayedFunctionLatentCommand( [=] {
                FHoudiniEngineTaskInfo CookTaskInfo;
                if( FHoudiniEngine::Get().RetrieveTaskInfo( CookGUID, CookTaskInfo ) )
                {
                    if( CookTaskInfo.TaskState != EHoudiniEngineTaskState::FinishedCooking )
                    {
                        AddError( FString::Printf( TEXT( "CookTaskInfo.Result: %d" ), (int32)CookTaskInfo.Result ) );
                    }
                    else
                    {
                        // Marshal the static mesh data
                        float GeoScale = 1.f;
                        if( const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >() )
                        {
                            GeoScale = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
                        }
                        TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesIn;
                        TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut;
                        FTestCookHandler CookHandler ( TestAsset );
                        CookHandler.HoudiniCookManager = &CookHandler;
                        CookHandler.StaticMeshBakeMode = EBakeMode::CookToTemp;
                        FTransform AssetTransform;
                        bool Result = FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset( 
                            InstantiateTaskInfo.AssetId,
                            CookHandler,
                            false, false, StaticMeshesIn, StaticMeshesOut, AssetTransform );

#define ExpectedNumVerts 24
#define ExpectedNumPoints 8
#define ExpectedNumTris 12

                        FHVert ExpectedVerts[ ExpectedNumVerts ] = {
                           {1, -0.0, -0.0, -1.0, 0.333333, 0.666667, 0.0},
                           {5, -0.0, 0.0, -1.0, 0.333333, 0.982283, 0.0},
                           {4, -0.0, -0.0, -1.0, 0.64895, 0.982283, 0.0},
                           {0, 0.0, -0.0, -1.0, 0.64895, 0.666667, 0.0},
                           {2, 1.0, -0.0, -0.0, 0.0, 0.666667, 0.0},
                           {6, 1.0, -0.0, -0.0, 0.0, 0.982283, 0.0},
                           {5, 1.0, 0.0, 0.0, 0.315616, 0.982283, 0.0},
                           {1, 1.0, -0.0, -0.0, 0.315616, 0.666667, 0.0},
                           {3, -0.0, -0.0, 1.0, 0.0, 0.333333, 0.0},
                           {7, -0.0, -0.0, 1.0, 0.0, 0.64895, 0.0},
                           {6, -0.0, -0.0, 1.0, 0.315616, 0.64895, 0.0},
                           {2, 0.0, 0.0, 1.0, 0.315616, 0.333333, 0.0},
                           {0, -1.0, 0.0, -0.0, 0.333333, 0.333333, 0.0},
                           {4, -1.0, -0.0, -0.0, 0.333333, 0.64895, 0.0},
                           {7, -1.0, -0.0, 0.0, 0.64895, 0.64895, 0.0},
                           {3, -1.0, -0.0, -0.0, 0.64895, 0.333333, 0.0},
                           {2, 0.0, -1.0, -0.0, 0.64895, 0.315616, 0.0},
                           {1, -0.0, -1.0, -0.0, 0.64895, 0.0, 0.0},
                           {0, -0.0, -1.0, 0.0, 0.333333, 0.0, 0.0},
                           {3, -0.0, -1.0, -0.0, 0.333333, 0.315616, 0.0},
                           {5, -0.0, 1.0, -0.0, 0.315616, 0.315616, 0.0},
                           {6, -0.0, 1.0, -0.0, 0.315616, 0.0, 0.0},
                           {7, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
                           {4, -0.0, 1.0, -0.0, 0.0, 0.315616, 0.0},
                        };

                        FVector ExpectedPoints[ ExpectedNumPoints ] = {
                           {-0.5, -0.5, -0.5},
                           {0.5, -0.5, -0.5},
                           {0.5, -0.5, 0.5},
                           {-0.5, -0.5, 0.5},
                           {-0.5, 0.5, -0.5},
                           {0.5, 0.5, -0.5},
                           {0.5, 0.5, 0.5},
                           {-0.5, 0.5, 0.5}
                        };

                        for( int32 Ix = 0; Ix < ExpectedNumPoints; ++Ix )
                        {
                            ExpectedPoints[ Ix ] *= GeoScale;
                        }

                        if( Result )
                        {
                            TestEqual( TEXT( "Num Mesh" ), StaticMeshesOut.Num(), 1 );
                            for( auto GeoPartSM : StaticMeshesOut )
                            {
                                FHoudiniGeoPartObject& Part = GeoPartSM.Key;
                                if( UStaticMesh* NewSM = GeoPartSM.Value )
                                {
                                    if( NewSM->RenderData->LODResources.Num() > 0 )
                                    {
                                        TestEqual( TEXT( "Num Triangles" ), ExpectedNumTris, NewSM->RenderData->LODResources[ 0 ].GetNumTriangles() );
                                        FPositionVertexBuffer& VB = NewSM->RenderData->LODResources[ 0 ].PositionVertexBuffer;
                                        const int32 VertexCount = VB.GetNumVertices();
                                        if( VertexCount !=  ExpectedNumVerts )
                                        {
                                            TestEqual(TEXT("Num Verts"), VertexCount, ExpectedNumVerts );
                                            break;
                                        }
                                        TArray<FVector> GeneratedPoints, ExpectedVertPositions;
                                        GeneratedPoints.SetNumUninitialized( VertexCount );
                                        ExpectedVertPositions.SetNumUninitialized( VertexCount );
                                        for( int32 Index = 0; Index < VertexCount; Index++ )
                                        {
                                            GeneratedPoints[Index] = VB.VertexPosition( Index );
                                            ExpectedVertPositions[ Index ] = ExpectedPoints[ ExpectedVerts[ Index ].PointNum ];
                                        }

                                        GeneratedPoints.Sort( FSortVectors() );
                                        ExpectedVertPositions.Sort( FSortVectors() );
                                        
                                        for( int32 Index = 0; Index < VertexCount; Index++ )
                                        {
                                            TestEqual( TEXT( "Points match" ), GeneratedPoints[Index], ExpectedVertPositions[Index] );
                                        }
                                    }
                                }
                                break;
                            }
                        }
                        else
                        {
                            AddError( FString::Printf( TEXT( "CreateStaticMeshesFromHoudiniAsset failed" )) );
                        }

                    }
                    UE_LOG( LogHoudiniTests, Log, TEXT( "CookTaskInfo.StatusText: %s" ), *CookTaskInfo.StatusText.ToString() );


                    FHoudiniEngine::Get().RemoveTaskInfo( CookGUID );
#if 1
                    // Now destroy asset
                    auto DelGUID = FGuid::NewGuid();
                    FHoudiniEngineTask DeleteTask( EHoudiniEngineTaskType::AssetDeletion, DelGUID );
                    DeleteTask.AssetId = InstantiateTaskInfo.AssetId;
                    FHoudiniEngine::Get().AddTask( DeleteTask );

                    AddCommand( new FDelayedFunctionLatentCommand( [=] {
                        FHoudiniEngineTaskInfo DeleteTaskInfo;
                        if( FHoudiniEngine::Get().RetrieveTaskInfo( DelGUID, DeleteTaskInfo ) )
                        {
                            // we don't have a task state to check since it's fire and forget
                            if( DeleteTaskInfo.Result != HAPI_RESULT_SUCCESS )
                            {
                                AddError( FString::Printf( TEXT( "DeleteTask.Result: %d" ), (int32)DeleteTaskInfo.Result ) );
                            }
                            UE_LOG( LogHoudiniTests, Log, TEXT( "DeleteTask.StatusText: %s" ), *DeleteTaskInfo.StatusText.ToString() );

                            FHoudiniEngine::Get().RemoveTaskInfo( DelGUID );
                        }

                    }, TestTickDelay ) );
#endif
                }

            }, TestTickDelay ) );

        }
    }, TestTickDelay ) );

    return true;
}

#endif // WITH_EDITOR
#endif // 0