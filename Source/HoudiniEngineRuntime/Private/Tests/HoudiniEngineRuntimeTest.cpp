#include "HoudiniApi.h"
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "FileCacheUtilities.h"
#include "StaticMeshResources.h"
#include "LevelEditorViewport.h"
#include "AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "Tests/AutomationCommon.h"
#include "IDetailsView.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniParamUtils.h"
#include "HoudiniCookHandler.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntimeTest.h"
#include "HoudiniAssetParameterInt.h"


DEFINE_LOG_CATEGORY_STATIC( LogHoudiniTests, Log, All );

static constexpr int32 kTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeMeshMarshalTest, "Houdini.Runtime.MeshMarshalTest", kTestFlags )
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeUploadStaticMeshTest, "Houdini.Runtime.UploadStaticMesh", kTestFlags )
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeActorTest, "Houdini.Runtime.ActorTest", kTestFlags )
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeParamTest, "Houdini.Runtime.ParamTest", kTestFlags )
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeBatchTest, "Houdini.Runtime.BatchTest", kTestFlags )

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

    virtual ~FTestCookHandler()
    {
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

UWorld* HelperGetWorld()
{
    return GWorld;
}

UObject* FindAssetUObject( FName AssetUObjectPath )
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
    TArray<FAssetData> AssetData;
    AssetRegistryModule.Get().GetAssetsByPackageName( AssetUObjectPath, AssetData );
    if( AssetData.Num() > 0 )
    {
        return AssetData[ 0 ].GetAsset();
    }
    return nullptr;
}

template<typename T>
T* HelperInstantiateAssetActor(
    FAutomationTestBase* Test,
    FName AssetPath )
{
    if( UHoudiniAsset* TestAsset = Cast<UHoudiniAsset>( FindAssetUObject( AssetPath ) ) )
    {
        GEditor->ClickLocation = FVector::ZeroVector;
        GEditor->ClickPlane = FPlane( GEditor->ClickLocation, FVector::UpVector );

        TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject( HelperGetWorld()->GetLevel( 0 ), TestAsset, true, RF_Transactional, nullptr );
        Test->TestTrue( TEXT( "Placed Actor" ), NewActors.Num() > 0 );
        if( NewActors.Num() > 0 )
        {
            return Cast<T>( NewActors[ 0 ] );
        }
    }
    return nullptr;
}

void HelperInstantiateAsset( 
    FAutomationTestBase* Test, 
    FName AssetPath, 
    TFunction<void( FHoudiniEngineTaskInfo, UHoudiniAsset* )> OnFinishedInstantiate )
{
    UHoudiniAsset* TestAsset = Cast<UHoudiniAsset>( FindAssetUObject(AssetPath) );
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

    Test->AddCommand( new FDelayedFunctionLatentCommand( [=]() {
        // check back on status on Instantiate
        FHoudiniEngineTaskInfo InstantiateTaskInfo = {};
        InstantiateTaskInfo.AssetId = -1;
        if( FHoudiniEngine::Get().RetrieveTaskInfo( InstGUID, InstantiateTaskInfo ) )
        {
            if( InstantiateTaskInfo.TaskState != EHoudiniEngineTaskState::FinishedInstantiation )
            {
                Test->AddError( FString::Printf( TEXT( "AssetInstantiation failed" ) ) );
            }
            UE_LOG( LogHoudiniTests, Log, TEXT( "InstantiateTask.StatusText: %s" ), *InstantiateTaskInfo.StatusText.ToString() );

            FHoudiniEngine::Get().RemoveTaskInfo( InstGUID );

            OnFinishedInstantiate( InstantiateTaskInfo, TestAsset );
        }
    }, 1.f ));
}

void HelperDeleteAsset( FAutomationTestBase* Test, HAPI_NodeId AssetId )
{
    // Now destroy asset
    auto DelGUID = FGuid::NewGuid();
    FHoudiniEngineTask DeleteTask( EHoudiniEngineTaskType::AssetDeletion, DelGUID );
    DeleteTask.AssetId = AssetId;
    FHoudiniEngine::Get().AddTask( DeleteTask );

    Test->AddCommand( new FDelayedFunctionLatentCommand( [=] {
        FHoudiniEngineTaskInfo DeleteTaskInfo;
        if( FHoudiniEngine::Get().RetrieveTaskInfo( DelGUID, DeleteTaskInfo ) )
        {
            // we don't have a task state to check since it's fire and forget
            if( DeleteTaskInfo.Result != HAPI_RESULT_SUCCESS )
            {
                Test->AddError( FString::Printf( TEXT( "DeleteTask.Result: %d" ), (int32)DeleteTaskInfo.Result ) );
            }
            UE_LOG( LogHoudiniTests, Log, TEXT( "DeleteTask.StatusText: %s" ), *DeleteTaskInfo.StatusText.ToString() );

            FHoudiniEngine::Get().RemoveTaskInfo( DelGUID );
        }

    }, 1.f ) );
}


void HelperCookAsset( 
    FAutomationTestBase* Test, 
    class UHoudiniAsset* InHoudiniAsset, 
    HAPI_NodeId AssetId,
    TFunction<void( bool, TMap< FHoudiniGeoPartObject, UStaticMesh * > )> OnFinishedCook )
{
    auto CookGUID = FGuid::NewGuid();
    FHoudiniEngineTask CookTask( EHoudiniEngineTaskType::AssetCooking, CookGUID );
    CookTask.AssetId = AssetId;

    FHoudiniEngine::Get().AddTask( CookTask );

    Test->AddCommand( new FDelayedFunctionLatentCommand( [=] {
        FHoudiniEngineTaskInfo CookTaskInfo;
        if( FHoudiniEngine::Get().RetrieveTaskInfo( CookGUID, CookTaskInfo ) )
        {
            bool CookSuccess = false;
            TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut;
            if( CookTaskInfo.TaskState != EHoudiniEngineTaskState::FinishedCooking )
            {
                Test->AddError( FString::Printf( TEXT( "CookTaskInfo.Result: %d, TaskState: %d" ), (int32)CookTaskInfo.Result, (int32)CookTaskInfo.TaskState ) );
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
                FTestCookHandler CookHandler ( InHoudiniAsset );
                CookHandler.HoudiniCookManager = &CookHandler;
                CookHandler.StaticMeshBakeMode = EBakeMode::CookToTemp;
                FTransform AssetTransform;
                CookSuccess = FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
                    AssetId,
                    CookHandler,
                    false, false, StaticMeshesIn, StaticMeshesOut, AssetTransform );
            }
            OnFinishedCook( CookSuccess, StaticMeshesOut );
        }
    }, 2.f ));
}

void HelperTestMeshEqual( FAutomationTestBase* Test, UStaticMesh* MeshA, UStaticMesh* MeshB )
{
    int32 InputNumVerts = 0;
    int32 InputNumTris = 0;
    
    Test->TestTrue( TEXT( "MeshA Valid" ), MeshA->RenderData->LODResources.Num() > 0 );

    if( MeshA->RenderData->LODResources.Num() > 0 )
    {
        FPositionVertexBuffer& VB = MeshA->RenderData->LODResources[ 0 ].VertexBuffers.PositionVertexBuffer;
        InputNumVerts = VB.GetNumVertices();
        InputNumTris = MeshA->RenderData->LODResources[ 0 ].GetNumTriangles();
    }

    Test->TestTrue( TEXT( "MeshB Valid" ), MeshB->RenderData->LODResources.Num() > 0 );
    if( MeshB->RenderData->LODResources.Num() > 0 )
    {
        Test->TestEqual( TEXT( "Num Triangles" ), InputNumTris, MeshB->RenderData->LODResources[ 0 ].GetNumTriangles() );
        FPositionVertexBuffer& VB = MeshB->RenderData->LODResources[ 0 ].VertexBuffers.PositionVertexBuffer;
        Test->TestEqual( TEXT( "Num Verts" ), VB.GetNumVertices(), InputNumVerts );
    }
}

bool FHoudiniEngineRuntimeParamTest::RunTest( const FString& Paramters )
{
    HelperInstantiateAsset( this, TEXT( "/HoudiniEngine/Test/TestParams" ),
        [=]( FHoudiniEngineTaskInfo InstantiateTaskInfo, UHoudiniAsset* HoudiniAsset )
    {
        HAPI_NodeId AssetId = InstantiateTaskInfo.AssetId;
        if( AssetId < 0 )
            return;

        UTestHoudiniParameterBuilder * Builder = NewObject<UTestHoudiniParameterBuilder>( HelperGetWorld(), TEXT("ParmBuilder"), RF_Standalone );
        bool Ok = FHoudiniParamUtils::Build( AssetId, Builder, Builder->CurrentParameters, Builder->NewParameters );
        TestTrue( TEXT( "Build success" ), Ok );
        if ( Ok )
        {
            // Look at old params
            TestEqual( TEXT( "No Old Parms" ), Builder->CurrentParameters.Num(), 0 );
            TestEqual( TEXT( "New Parms" ), Builder->NewParameters.Num(), 1 );

            // Look at new params
            for( auto& ParamPair : Builder->NewParameters )
            {
                UHoudiniAssetParameter* Param = ParamPair.Value;
                UE_LOG( LogHoudiniTests, Log, TEXT( "New Parm: %s" ), *Param->GetParameterName() );
                if( UHoudiniAssetParameterInt* ParamInt = Cast<UHoudiniAssetParameterInt>( Param ) )
                {
                    // Change a parameter

                    int32 Val = ParamInt->GetParameterValue( 0, -1 );
                    TestEqual( TEXT("GetParamterValue"), Val, 1 );
                    ParamInt->SetValue( 2, 0, false, false );
                    ParamInt->UploadParameterValue();
                    /*
                    ADD_LATENT_AUTOMATION_COMMAND( FTakeActiveEditorScreenshotCommand( TEXT( "FHoudiniEngineRuntimeParamTest_0.png" ) ));
                    //Wait so the screenshots have a chance to save
                    ADD_LATENT_AUTOMATION_COMMAND( FWaitLatentCommand( 0.1f ) );
                    */
                    break;
                }
                AddError( TEXT( "Didn't find Int Param" ) );
            }

            // Requery and verify param
            Builder->CurrentParameters = Builder->NewParameters;
            Builder->NewParameters.Empty();
            Ok = FHoudiniParamUtils::Build( AssetId, Builder, Builder->CurrentParameters, Builder->NewParameters );
            TestTrue( TEXT( "Build success2" ), Ok );
            if( Ok ) 
            {
                TestEqual( TEXT( "No Old Parms2" ), Builder->CurrentParameters.Num(), 0 );
                TestEqual( TEXT( "New Parms1" ), Builder->NewParameters.Num(), 1 );
                for( auto& ParamPair : Builder->NewParameters )
                {
                    UHoudiniAssetParameter* Param = ParamPair.Value;
                    UE_LOG( LogHoudiniTests, Log, TEXT( "New Parm: %s" ), *Param->GetParameterName() );
                    if( UHoudiniAssetParameterInt* ParamInt = Cast<UHoudiniAssetParameterInt>( Param ) )
                    {
                        int32 Val = ParamInt->GetParameterValue( 0, 0 );
                        TestEqual( TEXT( "GetParamterValue2" ), Val, 2 );
                        break;
                    }
                    AddError( TEXT("Didn't find Int Param2") );
                }
            }
        }

        Builder->ConditionalBeginDestroy();
        
        HelperCookAsset( this, HoudiniAsset, AssetId,
            [=]( bool CookOk, TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut )
        {
            if( CookOk )
            {
                TestEqual( TEXT( "Num Mesh" ), StaticMeshesOut.Num(), 1 );
            }

            HelperDeleteAsset( this, AssetId );
        } );


    } );

    return true;
}

bool FHoudiniEngineRuntimeBatchTest::RunTest( const FString& Paramters )
{
    HelperInstantiateAsset( this, TEXT( "/HoudiniEngine/Test/TestPolyReduce" ),
        [=]( FHoudiniEngineTaskInfo InstantiateTaskInfo, UHoudiniAsset* HoudiniAsset )
    {
        HAPI_NodeId AssetId = InstantiateTaskInfo.AssetId;
        if( AssetId < 0 )
            return;

        UTestHoudiniParameterBuilder * Builder = NewObject<UTestHoudiniParameterBuilder>( HelperGetWorld(), TEXT( "ParmBuilder" ), RF_Standalone );
        bool Ok = FHoudiniParamUtils::Build( AssetId, Builder, Builder->CurrentParameters, Builder->NewParameters );
        TestTrue( TEXT( "Build success" ), Ok );
        if( Ok )
        {
            // Build the inputs
            UHoudiniAssetInput* GeoInputParm = UHoudiniAssetInput::Create( Builder, 0, AssetId );
            TestTrue( TEXT( "Found Input" ), GeoInputParm != nullptr );

            int32 InputNumTris = 0;
            UStaticMesh * GeoInput = Cast<UStaticMesh>( StaticLoadObject(
                UObject::StaticClass(), nullptr, TEXT( "StaticMesh'/Engine/BasicShapes/Cube.Cube'" ), nullptr, LOAD_None, nullptr ) );

            TestTrue( TEXT( "Load Input Mesh" ), GeoInput != nullptr );
            if( ! GeoInput )
                return;

            if( GeoInput->RenderData->LODResources.Num() > 0 )
            {
                InputNumTris = GeoInput->RenderData->LODResources[ 0 ].GetNumTriangles();
            }
            TestTrue( TEXT( "Find Geo Input" ), GeoInputParm != nullptr );

            for( int32 Ix = 0; Ix < 3; ++Ix )
            {
                GEditor->ClickLocation = FVector(0, Ix * 200, 0);
                GEditor->ClickPlane = FPlane( GEditor->ClickLocation, FVector::UpVector );

                TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject( HelperGetWorld()->GetLevel( 0 ), GeoInput, true, RF_Transactional, nullptr );
                TestTrue( TEXT( "Placed Actor" ), NewActors.Num() > 0 );

                if( GeoInputParm && NewActors.Num() > 0 )
                {
                    AActor* SMActor = NewActors.Pop();
                    GeoInputParm->ClearInputs();
                    GeoInputParm->ForceSetInputObject( SMActor, 0, true );  // triggers upload of data

                    auto CookGUID = FGuid::NewGuid();
                    FHoudiniEngineTask CookTask( EHoudiniEngineTaskType::AssetCooking, CookGUID );
                    CookTask.AssetId = AssetId;

                    FHoudiniEngine::Get().AddTask( CookTask );

                    while( true )
                    {
                        FHoudiniEngineTaskInfo CookTaskInfo;
                        if( FHoudiniEngine::Get().RetrieveTaskInfo( CookGUID, CookTaskInfo ) )
                        {
                            bool CookSuccess = false;
                            TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut;
                            if( CookTaskInfo.TaskState == EHoudiniEngineTaskState::Processing )
                            {
                                FPlatformProcess::Sleep( 1 );
                                continue;
                            }
                            else if( CookTaskInfo.TaskState == EHoudiniEngineTaskState::FinishedCookingWithErrors )
                            {
                                AddError( FString::Printf( TEXT( "CookTaskInfo.Result: %d, TaskState: %d" ), (int32)CookTaskInfo.Result, (int32)CookTaskInfo.TaskState ) );
                                break;
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
                                FTestCookHandler CookHandler ( HoudiniAsset );
                                CookHandler.HoudiniCookManager = &CookHandler;
                                CookHandler.StaticMeshBakeMode = EBakeMode::CookToTemp;
                                FTransform AssetTransform;
                                CookSuccess = FHoudiniEngineUtils::CreateStaticMeshesFromHoudiniAsset(
                                    AssetId,
                                    CookHandler,
                                    false, false, StaticMeshesIn, StaticMeshesOut, AssetTransform );

                                if( CookSuccess && StaticMeshesOut.Num() > 0)
                                {
                                    for( auto SMPair : StaticMeshesOut )
                                    {
                                        int32 OutputNumTris = SMPair.Value->RenderData->LODResources[ 0 ].GetNumTriangles();
                                        TestTrue( TEXT( "reduce worked" ), OutputNumTris < InputNumTris );
                                    }
                                }
                                else
                                {
                                    AddError( TEXT( "Failed to get static mesh output" ) );
                                }
                            }
                            break;
                        }
                        else
                        {
                            AddError( TEXT( "Failed to get task state" ) );
                            break;
                        }
                    }
                }
            }
            Builder->ConditionalBeginDestroy();
            HelperDeleteAsset( this, AssetId );
        }
    } );

    return true;
}

bool FHoudiniEngineRuntimeActorTest::RunTest( const FString& Paramters )
{
    AHoudiniAssetActor* Actor = HelperInstantiateAssetActor<AHoudiniAssetActor>( this, TEXT( "/HoudiniEngine/Test/InputEcho" ) );
    AddCommand( new FDelayedFunctionLatentCommand( [=] {
        if( Actor && Actor->GetHoudiniAssetComponent() )
        {
            UHoudiniAssetComponent* Comp = Actor->GetHoudiniAssetComponent();
            TestEqual( TEXT( "Done Cooking" ), Comp->IsInstantiatingOrCooking(), false );

            FPropertyEditorModule & PropertyModule =
                FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

            // Locate the details panel.
            FName DetailsPanelName = "LevelEditorSelectionDetails";
            TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView( DetailsPanelName );

            if( DetailsView.IsValid() )
            {
                /*
                auto DetailsW = StaticCastSharedRef<SWidget>( DetailsView->AsShared() );
                if( FAutomationTestFramework::Get().IsScreenshotAllowed() )
                {
                    const FString TestName = TEXT( "HoudiniEngineRuntimeActorTest_0.png" );
                    TArray<FColor> OutImageData;
                    FIntVector OutImageSize;
                    if( FSlateApplication::Get().TakeScreenshot( DetailsW, OutImageData, OutImageSize ) )
                    {
                        FAutomationScreenshotData Data;
                        Data.Width = OutImageSize.X;
                        Data.Height = OutImageSize.Y;
                        Data.Path = TestName;
                        FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound( OutImageData, Data );
                    }
                }
                */
            }

        }
    }, 1.5f ));
    return true;
}

bool FHoudiniEngineRuntimeUploadStaticMeshTest::RunTest( const FString& Parameters )
{
    HelperInstantiateAsset( this, TEXT( "/HoudiniEngine/Test/InputEcho"), 
        [=]( FHoudiniEngineTaskInfo InstantiateTaskInfo, UHoudiniAsset* HoudiniAsset ) 
    {
        HAPI_NodeId AssetId = InstantiateTaskInfo.AssetId;

        if( AssetId < 0 )
        {
            return;
        }

        TArray<UObject *> InputObjects;
        TArray< FTransform > InputTransforms;

        HAPI_NodeId ConnectedAssetId;
        TArray< HAPI_NodeId > GeometryInputAssetIds;

        UStaticMesh * GeoInput = Cast<UStaticMesh>( StaticLoadObject(
                UObject::StaticClass(), nullptr, TEXT( "StaticMesh'/Engine/BasicShapes/Cube.Cube'" ), nullptr, LOAD_None, nullptr ));

        TestTrue( TEXT("Load Input Mesh"), GeoInput != nullptr );
        if( ! GeoInput )
            return;

        InputObjects.Add( GeoInput );
        InputTransforms.Add( FTransform::Identity );

        if( ! FHoudiniEngineUtils::HapiCreateInputNodeForObjects( AssetId, InputObjects, InputTransforms, ConnectedAssetId, GeometryInputAssetIds, false ) )
        {
            AddError( FString::Printf( TEXT( "HapiCreateInputNodeForData failed" )));
        }

        // Now connect the input
        int32 InputIndex = 0;
        HAPI_Result Result = FHoudiniApi::ConnectNodeInput(
            FHoudiniEngine::Get().GetSession(), AssetId, InputIndex,
            ConnectedAssetId, 0 );

        TestEqual( TEXT("ConnectNodeInput"), HAPI_RESULT_SUCCESS, Result );

        HelperCookAsset( this, HoudiniAsset, AssetId, 
            [=]( bool Ok, TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut )
        {

            if( Ok )
            {
                TestEqual( TEXT( "Num Mesh" ), StaticMeshesOut.Num(), 1 );
            }

            for( auto GeoPartSM : StaticMeshesOut )
            {
                FHoudiniGeoPartObject& Part = GeoPartSM.Key;
                if( UStaticMesh* NewSM = GeoPartSM.Value )
                {
                    HelperTestMeshEqual( this, GeoInput, NewSM );
                }
                break;
            }

            HelperDeleteAsset( this, AssetId );
        } );

    } );
    return true;
}

bool FHoudiniEngineRuntimeMeshMarshalTest::RunTest( const FString& Parameters )
{
    HelperInstantiateAsset( this, TEXT( "/HoudiniEngine/Test/TestBox" ),
        [=]( FHoudiniEngineTaskInfo InstantiateTaskInfo, UHoudiniAsset* HoudiniAsset )
    {
        HAPI_NodeId AssetId = InstantiateTaskInfo.AssetId;

        if( AssetId < 0 )
        {
            return;
        }

        HelperCookAsset( this, HoudiniAsset, AssetId,
            [=]( bool Ok, TMap< FHoudiniGeoPartObject, UStaticMesh * > StaticMeshesOut )
        {

            if( !Ok )
            {
                return;
            }

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

            // Scale our expected data into unreal space
            float GeoScale = 1.f;
            if( const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >() )
            {
                GeoScale = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
            }
            for( int32 Ix = 0; Ix < ExpectedNumPoints; ++Ix )
            {
                ExpectedPoints[ Ix ] *= GeoScale;
            }


            TestEqual( TEXT( "Num Mesh" ), StaticMeshesOut.Num(), 1 );
            for( auto GeoPartSM : StaticMeshesOut )
            {
                FHoudiniGeoPartObject& Part = GeoPartSM.Key;
                if( UStaticMesh* NewSM = GeoPartSM.Value )
                {
                    if( NewSM->RenderData->LODResources.Num() > 0 )
                    {
                        TestEqual( TEXT( "Num Triangles" ), ExpectedNumTris, NewSM->RenderData->LODResources[ 0 ].GetNumTriangles() );
                        FPositionVertexBuffer& VB = NewSM->RenderData->LODResources[ 0 ].VertexBuffers.PositionVertexBuffer;
                        const int32 VertexCount = VB.GetNumVertices();
                        if( VertexCount !=  ExpectedNumVerts )
                        {
                            TestEqual( TEXT( "Num Verts" ), VertexCount, ExpectedNumVerts );
                            break;
                        }
                        TArray<FVector> GeneratedPoints, ExpectedVertPositions;
                        GeneratedPoints.SetNumUninitialized( VertexCount );
                        ExpectedVertPositions.SetNumUninitialized( VertexCount );
                        for( int32 Index = 0; Index < VertexCount; Index++ )
                        {
                            GeneratedPoints[ Index ] = VB.VertexPosition( Index );
                            ExpectedVertPositions[ Index ] = ExpectedPoints[ ExpectedVerts[ Index ].PointNum ];
                        }

                        GeneratedPoints.Sort( FSortVectors() );
                        ExpectedVertPositions.Sort( FSortVectors() );

                        for( int32 Index = 0; Index < VertexCount; Index++ )
                        {
                            TestEqual( TEXT( "Points match" ), GeneratedPoints[ Index ], ExpectedVertPositions[ Index ] );
                        }
                    }
                }
                break;
            }

            HelperDeleteAsset( this, AssetId );
        } );
    });
    return true;
}

#endif // WITH_EDITOR