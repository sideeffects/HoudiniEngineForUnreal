/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#pragma once

#include "Commandlets/Commandlet.h"
#include "Logging/LogMacros.h"
#include "HoudiniEngineCommandlet.generated.h"

/** Declares a log category for this module. */
//DECLARE_LOG_CATEGORY_EXTERN( LogHoudiniEngineCommandlet, Log, All );

class UStaticMesh;
struct FHoudiniGeoPartObject;

struct FHoudiniCommandletUtils
{
    static bool ConvertBGEOFileToUAsset( const FString& InBGEOFilePath, const FString& OutUAssetFilePath );

    static bool LoadBGEOFileInHAPI( const FString& InputFilePath, HAPI_NodeId& NodeId );

    static bool CreateStaticMeshes(
	const FString& InputName, HAPI_NodeId& NodeId, UPackage* OuterPackage,
	TMap<FHoudiniGeoPartObject, UStaticMesh *>& StaticMeshesOut );

    static bool BakeStaticMeshesToPackage(
	const FString& InputName, TMap<FHoudiniGeoPartObject, UStaticMesh *>& StaticMeshes, UPackage* OutPackage );
};

UCLASS()
class UHoudiniEngineConvertBgeoCommandlet : public UCommandlet
{
    GENERATED_BODY()
    public:

	/** Default constructor. */
	UHoudiniEngineConvertBgeoCommandlet();

    public:

	//~ UCommandlet interface
	virtual int32 Main(const FString& Params) override;
};

UCLASS()
class UHoudiniEngineConvertBgeoDirCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:

    /** Default constructor. */
    UHoudiniEngineConvertBgeoDirCommandlet();

public:

    //~ UCommandlet interface
    virtual int32 Main(const FString& Params) override;
};