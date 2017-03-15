#include "HoudiniApi.h"
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FileCacheUtilities.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniEngineUtils.h"
#include "AssetRegistryModule.h"
#include "HoudiniCookHandler.h"

DEFINE_LOG_CATEGORY_STATIC( LogHoudiniTests, Log, All );

/** Test MatchExtensionString */
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FHoudiniEngineRuntimeInstantiateAssetTest, "Houdini.Runtime.InstantiateAsset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter )

static float TestTickDelay = 2.0f;

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

bool FHoudiniEngineRuntimeInstantiateAssetTest::RunTest( const FString& Parameters )
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
    TArray<FAssetData> AssetData;
    AssetRegistryModule.Get().GetAssetsByPackageName( TEXT("/Game/ItsATorusGeo"), AssetData );
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

                    }
                    UE_LOG( LogHoudiniTests, Log, TEXT( "CookTaskInfo.StatusText: %s" ), *CookTaskInfo.StatusText.ToString() );


                    FHoudiniEngine::Get().RemoveTaskInfo( CookGUID );
#if 0
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