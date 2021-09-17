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

#include "CoreMinimal.h"
#include "UObject/Class.h"

struct HOUDINIENGINE_API FHoudiniEngineOutputStats
{
	FHoudiniEngineOutputStats();
	
	int32 NumPackagesCreated;
	int32 NumPackagesUpdated;

	// These FStrings should preferably be EHoudiniOutputType enum
	// Move the OUtput enums into a separate header to avoid circular dependencies.
	TMap<FString, int32> OutputObjectsCreated;
	TMap<FString, int32> OutputObjectsUpdated;
	TMap<FString, int32> OutputObjectsReplaced;

	void NotifyPackageCreated(int32 NumCreated);
	void NotifyPackageUpdated(int32 NumUpdated);

	// Objects created
	void NotifyObjectsCreated(const FString& ObjectTypeName, int32 NumCreated);
	template<typename EnumT>
	void NotifyObjectsCreated(EnumT EnumValue, int32 NumCreated)
	{
		NotifyObjectsCreated( UEnum::GetValueAsString(EnumValue), NumCreated );
	}

	// Object updated
	void NotifyObjectsUpdated(const FString& ObjectTypeName, int32 NumUpdated);
	template<typename EnumT>
	void NotifyObjectsUpdated(EnumT EnumValue, int32 NumUpdated)
	{
		NotifyObjectsUpdated( UEnum::GetValueAsString(EnumValue), NumUpdated );
	}

	// Objects replaced
	void NotifyObjectsReplaced(const FString& ObjectTypeName, int32 NumReplaced);
	template<typename EnumT>
	void NotifyObjectsReplaced(EnumT EnumValue, int32 NumReplaced)
	{
		NotifyObjectsReplaced( UEnum::GetValueAsString(EnumValue), NumReplaced );
	}
};
