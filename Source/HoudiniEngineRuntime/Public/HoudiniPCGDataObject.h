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
#include <Data/PCGSplineData.h>
#include "HoudiniPCGDataObject.generated.h"

class UPCGPointData;
class FUnrealObjectInputHandle;

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeBase : public UObject
{
    GENERATED_BODY()
public:

    void SetAttrName(const FString& Name);
    const FName& GetAttrName() const;

    virtual int GetNumValues() const { return 0; }
private:
    UPROPERTY()
    FName AttrName;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeFloat : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()
public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<float> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeDouble : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()

public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<float> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeInt : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()
public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<int> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeInt64 : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()
public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<int64> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeVector2d : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()
public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<FVector2D> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeVector3d : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()
public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<FVector> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeQuat : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()

public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<FQuat> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeVector4d : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()

public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<FVector4d> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeString: public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()

public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<FString> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeSoftObjectPath : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()

public:
    virtual int GetNumValues() const override { return Values.Num(); }

    UPROPERTY()
    TArray<FSoftObjectPath> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataAttributeSoftClassPath : public UHoudiniPCGDataAttributeBase
{
    GENERATED_BODY()

public:
    virtual int GetNumValues() const override { return Values.Num(); }
    
    UPROPERTY()
    TArray<FSoftClassPath> Values;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataObject : public UObject
{
    // This class serves a cache of UPCGData (Point or Param) data. We copy it out as
    // the Unreal data is not serializable.  Possibly
    // this is not needed is we just store a CRC, and not the data... but this also
    // protects us from API changes.

    GENERATED_BODY()
public:
    void SetFromPCGData(const UPCGData* PCGParamaData, const TSet<FString> & Tags = {});
    void SetFromPCGData(const UPCGParamData* PCGParamaData);
    void SetFromPCGData(const UPCGSplineData* PCGSplineData);
    void SetFromPCGData(const UPCGPointData* PCGPointData);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    void SetFromPCGBasePointData(const UPCGBasePointData* PCGParamaData);
    void SetFromPCGData(const UPCGPointArrayData* PCGParamaData);
#endif

    bool operator==(const UHoudiniPCGDataObject& Other) const;
    bool operator!=(const UHoudiniPCGDataObject& Other) const;
    int GetNumRows() const;

    UHoudiniPCGDataAttributeBase* FindAttribute(const FString& AttrName);

    UPROPERTY()
    TArray<TObjectPtr<UHoudiniPCGDataAttributeBase>> Attributes;

    UPROPERTY()
    EPCGDataType PCGDataType;

    UPROPERTY()
    TSet<FString> PCGTags;

    UPROPERTY()
    bool bIsClosed = false;

private:
	void AddMetaDataAttributes(const UPCGMetadata* PCGParamaData, const TArray<int64> & Keys);

    UHoudiniPCGDataAttributeString* CreateAttributeString(const FString& AttributeName);
    UHoudiniPCGDataAttributeVector4d* CreateAttributeVector4d(const FString& AttributeName);
    UHoudiniPCGDataAttributeFloat* CreateAttributeFloat(const FString& AttributeName);
    UHoudiniPCGDataAttributeDouble* CreateAttributeDouble(const FString& AttributeName);
    UHoudiniPCGDataAttributeInt* CreateAttributeInt(const FString& AttributeName);
    UHoudiniPCGDataAttributeInt64* CreateAttributeInt64(const FString& AttributeName);
    UHoudiniPCGDataAttributeVector2d* CreateAttributeVector2d(const FString& AttributeName);
    UHoudiniPCGDataAttributeVector3d* CreateAttributeVector3d(const FString& AttributeName);
    UHoudiniPCGDataAttributeQuat* CreateAttributeQuat(const FString& AttributeName);
    UHoudiniPCGDataAttributeSoftObjectPath* CreateAttributeSoftObjectPath(const FString& AttributeName);
    UHoudiniPCGDataAttributeSoftClassPath* CreateAttributeSoftClassPath(const FString& AttributeName);

};

UENUM()
enum class EHoudiniPCGDataType
{
    InputPCGNone,
	InputPCGGeometry,
    InputPCGSplines
};
UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGDataCollection : public UObject
{
    GENERATED_BODY()

public:

    bool operator==(const UHoudiniPCGDataCollection& Other) const;
    bool operator!=(const UHoudiniPCGDataCollection& Other) const;

    void AddObject(UHoudiniPCGDataObject * Object);

    UPROPERTY()
    EHoudiniPCGDataType Type;

    UPROPERTY()
    TObjectPtr<UHoudiniPCGDataObject> Details;

    UPROPERTY()
    TObjectPtr<UHoudiniPCGDataObject> Primitives;


    UPROPERTY()
    TObjectPtr<UHoudiniPCGDataObject> Vertices;

    UPROPERTY()
    TObjectPtr<UHoudiniPCGDataObject> Points;

    UPROPERTY()
    TArray<TObjectPtr<UHoudiniPCGDataObject>> Splines;

};

class UPCGMetadata;
class UPCGParamData;

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPCGOutputData : public UObject
{
public:
    GENERATED_UCLASS_BODY()

    UPROPERTY()
    TObjectPtr<UPCGParamData> DetailsParams = nullptr;

    UPROPERTY()
    TObjectPtr<UPCGParamData> PrimsParams = nullptr;

    UPROPERTY()
    TObjectPtr<UPCGParamData> VertexParams = nullptr;

    UPROPERTY()
    TObjectPtr<UPCGPointData> PointParams = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<UPCGSplineData>> SplineParams;

};