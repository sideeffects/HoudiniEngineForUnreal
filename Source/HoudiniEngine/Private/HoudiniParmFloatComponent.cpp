#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniParmFloatComponent.h"
#include "HAPI.h"
#include <string>

UHoudiniParmFloatComponent::UHoudiniParmFloatComponent( const FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
{
	PrimaryComponentTick.bCanEverTick = false;

}