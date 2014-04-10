#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniAssetActor.h"
#include "Engine.h"
#include "HAPI.h"
#include <string>

AHoudiniAssetActor::AHoudiniAssetActor( const FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
	, myAssetId( -1 )
{
	myAssetComponent = PCIP.CreateDefaultSubobject< UHoudiniAssetComponent >( this, TEXT( "HoudiniAssetComponent" ) );
	RootComponent = myAssetComponent;
	SerializedComponents.Add( myAssetComponent );
}

void AHoudiniAssetActor::PostActorCreated()
{
	Super::PostActorConstruction();

	UE_LOG(
		LogHoudiniEngine, Log,
		TEXT(
			"Actor::PostActorCreated-BeforeCook: id: %d, public_path: %s" ),
			myAssetId, *AssetLibraryPath );

	myAssetComponent->AssetLibraryPath = AssetLibraryPath;
	myAssetComponent->DoWork();
	myAssetId = myAssetComponent->GetAssetId();

	UE_LOG(
		LogHoudiniEngine, Log,
		TEXT(
			"Actor::PostActorCreated-AfterCook: id: %d, public_path: %s" ),
			myAssetId, *AssetLibraryPath );
}

void AHoudiniAssetActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	UE_LOG(
		LogHoudiniEngine, Log,
		TEXT(
			"Actor::PostRegisterAllComponents-BeforeCook: id: %d, public_path: %s" ),
			myAssetId, *AssetLibraryPath );
}
