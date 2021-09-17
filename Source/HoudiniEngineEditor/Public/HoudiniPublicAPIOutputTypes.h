/*
* Copyright (c) <2018> Side Effects Software Inc.
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

#include "HoudiniEngineRuntimeCommon.h"

#include "HoudiniPublicAPIOutputTypes.generated.h"


struct FHoudiniOutputObjectIdentifier;

/**
 * This class represents an output object identifier for an output object of a wrapped Houdini asset in the
 * public API.
 */
USTRUCT(BlueprintType, Category="Houdini Engine | Public API | Outputs")
struct FHoudiniPublicAPIOutputObjectIdentifier
{
	GENERATED_BODY()

public:
	FHoudiniPublicAPIOutputObjectIdentifier();

	FHoudiniPublicAPIOutputObjectIdentifier(const FHoudiniOutputObjectIdentifier& InIdentifier);

	/** Returns the internal output object identifier wrapped by this class. */
	FHoudiniOutputObjectIdentifier GetIdentifier() const;

	/**
	 * Sets the internal output object identifier wrapped by this class.
	 * @param InIdentifier The internal output object identifier.
	 */
	void SetIdentifier(const FHoudiniOutputObjectIdentifier& InIdentifier);

	/** String identifier for the split that created the output object identified by this identifier. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Houdini Engine | Public API | Outputs")
	FString SplitIdentifier;

	/** Name of the part used to generate the output object identified by this identifier. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Houdini Engine | Public API | Outputs")
	FString PartName;

protected:

	// NodeId of corresponding Houdini Object.
	UPROPERTY()
	int32 ObjectId = -1;

	// NodeId of corresponding Houdini Geo.
	UPROPERTY()
	int32 GeoId = -1;

	// PartId
	UPROPERTY()
	int32 PartId = -1;

	// First valid primitive index for this output
	// (used to read generic attributes)
	UPROPERTY()
	int32 PrimitiveIndex = -1;

	// First valid point index for this output
	// (used to read generic attributes)
	UPROPERTY()
	int32 PointIndex = -1;

	bool bLoaded = false;
};
