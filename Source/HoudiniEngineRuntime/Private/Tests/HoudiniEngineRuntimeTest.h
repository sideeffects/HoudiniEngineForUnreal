#pragma once

#include "CoreUObject.h"


#include "HoudiniEngineRuntimeTest.generated.h"

UCLASS()
class UTestHoudiniParameterBuilder : public UObject
{
    GENERATED_BODY()
public:
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * > NewParameters;
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * > CurrentParameters;
};
