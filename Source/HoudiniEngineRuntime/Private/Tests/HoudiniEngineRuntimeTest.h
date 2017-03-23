#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

#include "HoudiniEngineRuntimeTest.generated.h"

UCLASS()
class UTestHoudiniParameterBuilder : public UObject
{
    GENERATED_BODY()
public:
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * > NewParameters;
    TMap< HAPI_ParmId, class UHoudiniAssetParameter * > CurrentParameters;
};
