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

#include "HoudiniPublicAPIOutputTypes.h"

#include "HoudiniOutput.h"

FHoudiniPublicAPIOutputObjectIdentifier::FHoudiniPublicAPIOutputObjectIdentifier()
	: SplitIdentifier()
	, PartName()
	, ObjectId(-1)
	, GeoId(-1)
	, PartId(-1)

	, PrimitiveIndex(-1)
	, PointIndex(-1)
	, bLoaded(false)
{
}

FHoudiniPublicAPIOutputObjectIdentifier::FHoudiniPublicAPIOutputObjectIdentifier(const FHoudiniOutputObjectIdentifier& InIdentifier)
	: SplitIdentifier()
	, PartName()
{
	SetIdentifier(InIdentifier);
}

void
FHoudiniPublicAPIOutputObjectIdentifier::SetIdentifier(const FHoudiniOutputObjectIdentifier& InIdentifier)
{
	ObjectId = InIdentifier.ObjectId;
	GeoId = InIdentifier.GeoId;
	PartId = InIdentifier.PartId;
	SplitIdentifier = InIdentifier.SplitIdentifier;
	PartName = InIdentifier.PartName;
	PrimitiveIndex = InIdentifier.PrimitiveIndex;
	PointIndex = InIdentifier.PointIndex;
	bLoaded = InIdentifier.bLoaded;
}

/** Returns the internal output object identifier wrapped by this class. */
FHoudiniOutputObjectIdentifier 
FHoudiniPublicAPIOutputObjectIdentifier::GetIdentifier() const 
{
	FHoudiniOutputObjectIdentifier Identifier;
	Identifier.ObjectId = ObjectId;
	Identifier.GeoId = GeoId;
	Identifier.PartId = PartId;
	Identifier.SplitIdentifier = SplitIdentifier;
	Identifier.PartName = PartName;
	Identifier.PrimitiveIndex = PrimitiveIndex;
	Identifier.PointIndex = PointIndex;
	Identifier.bLoaded = bLoaded;

	return Identifier; 
}
