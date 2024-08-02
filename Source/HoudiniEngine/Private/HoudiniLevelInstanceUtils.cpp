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

#include "HoudiniLevelInstanceUtils.h"
#include "Editor.h"
#include "HoudiniEngineAttributes.h"
#include "HoudiniEngineUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "HoudiniOutput.h"

TOptional<FHoudiniLevelInstanceParams> FHoudiniLevelInstanceUtils::GetParams(int NodeId, int PartId)
{
	// See if a level instance was specified. If it wasn't ignore.
	FString OutputName;
	FHoudiniHapiAccessor Accessor(NodeId, PartId, HAPI_UNREAL_ATTRIB_LEVEL_INSTANCE_NAME);
	bool bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, OutputName);

	if (!bSuccess || OutputName.IsEmpty())
		return {};

	// Create default params.
	FHoudiniLevelInstanceParams Params;
	Params.OutputName = OutputName;
	Params.Type = ELevelInstanceCreationType::LevelInstance;

	// See if a type was specified.
	int32 Type = 0;

	Accessor.Init(NodeId, PartId, HAPI_UNREAL_ATTRIB_LEVEL_INSTANCE_TYPE);
	bSuccess = Accessor.GetAttributeFirstValue(HAPI_ATTROWNER_INVALID, Type);

	if (bSuccess)
	{
		Params.Type = Type == 0 ? ELevelInstanceCreationType::LevelInstance : ELevelInstanceCreationType::PackedLevelActor;
	}

	return Params;

}

uint32 GetTypeHash(const FHoudiniLevelInstanceParams& Params)
{
	return (GetTypeHash((int32)Params.Type) + 23 * GetTypeHash(Params.OutputName));
}

bool operator==(const FHoudiniLevelInstanceParams & p1, const FHoudiniLevelInstanceParams & p2)
{
	return p1.Type == p2.Type && p1.OutputName == p2.OutputName;
}

bool FHoudiniLevelInstanceUtils::FetchLevelInstanceParameters(TArray<UHoudiniOutput*>& CookedOutputs)
{
	for (int Index = 0; Index < CookedOutputs.Num(); Index++)
	{
		UHoudiniOutput& CookedOutput = *CookedOutputs[Index];
		for(auto & It : CookedOutput.GetOutputObjects())
		{
			TOptional<FHoudiniLevelInstanceParams> Params = GetParams(It.Key.GeoId, It.Key.PartId);
			if (Params.IsSet())
			{
				It.Value.LevelInstanceParams = Params.GetValue();
			}
		}
	}
	return true;
}


