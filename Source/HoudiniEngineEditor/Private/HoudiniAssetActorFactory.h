/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "HoudiniAssetActorFactory.generated.h"

class FText;
class AActor;
class UObject;
class UHoudiniAssetComponent;

struct FAssetData;

UCLASS(config = Editor)
class UHoudiniAssetActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

public:
	// UActorFactory methods:
	// Return true if Actor can be created from a given asset.
	virtual bool CanCreateActorFrom(const FAssetData & AssetData, FText & OutErrorMsg) override;
	// Given an instance of an actor pertaining to this factory, find the asset that should be used to create a new actor.
	virtual UObject * GetAssetFromActorInstance(AActor * Instance) override;
	// Modify the actor after it has been spawned.
	virtual void PostSpawnActor(UObject * Asset, AActor * NewActor) override;
	// Called after a blueprint is created by this factory to update the blueprint's CDO properties
	// with state from the asset for this factory.
	virtual void PostCreateBlueprint(UObject * Asset, AActor * CDO) override;

protected:
	bool AddHoudiniLogoToComponent(UHoudiniAssetComponent* HAC);
};
