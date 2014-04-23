/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once
#include "HoudiniEngineAssetFactory.generated.h"

UCLASS()
class UHoudiniEngineAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

private: /** Overriding UFactory methods **/

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) OVERRIDE;
	virtual FText GetDisplayName() const OVERRIDE;
};
