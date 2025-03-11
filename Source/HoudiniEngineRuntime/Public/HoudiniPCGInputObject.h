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

#include <PCGData.h>
#include "UObject/NameTypes.h"
#include "HoudiniPCGInputObject.generated.h"

class UPCGPointData;
class FUnrealObjectInputHandle;

class HOUDINIENGINERUNTIME_API FHoudiniPCGInputAttributeDataBase
{
public:
    FName Name;
    int DataType;
    virtual ~FHoudiniPCGInputAttributeDataBase() {};
    virtual int GetNumValues() const { return 0; }
};

template<typename Type>

class HOUDINIENGINERUNTIME_API FHoudiniPCGInputAttributeData : public FHoudiniPCGInputAttributeDataBase
{
public:

    FHoudiniPCGInputAttributeData(const FName& AttributeName);
    int GetNumValues() const override { return Values.Num();  }
    TArray<Type> Values;
    static int StaticType;

};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGInputObject : public UObject
{
    // This class serves a cache of UPCGData (Point or Param) data. Its possible we could use PCG's
    // object directly, but since efficient cooking relies on detecting when inputs have changed
    // we really what the test to be accurate, and its possible the UPCGParam data struct might
    // change even if the data hasn't really. So we copy the data out here and pass it to the
    // input system to see if its changed or not.

    GENERATED_BODY()
public:
    void Initialize(const UPCGData* PCGParamaData);

    TArray<TUniquePtr<FHoudiniPCGInputAttributeDataBase>> Attributes;
    bool operator==(const UHoudiniPCGInputObject& Other) const;
    bool operator!=(const UHoudiniPCGInputObject& Other) const;

private:
    void Initialize(const UPCGParamData* PCGParamaData);
    void AddMetaDataAttributes(const UPCGMetadata* PCGParamaData);
    void Initialize(const UPCGPointData* PCGParamaData);
};
