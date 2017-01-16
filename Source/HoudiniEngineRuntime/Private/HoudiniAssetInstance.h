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

#include "HoudiniGeoPartObject.h"
#include "HoudiniParameterObject.h"
#include "HoudiniInputObject.h"
#include "HoudiniAssetInstance.generated.h"

struct FTransform;
class UHoudiniAsset;
class FHoudiniEngineString;

UCLASS(EditInlineNew, config=Engine)
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstance : public UObject
{
    GENERATED_UCLASS_BODY()

    public:

        virtual ~UHoudiniAssetInstance();

    public:

        /** Create an asset instance. **/
        static UHoudiniAssetInstance * CreateAssetInstance( UObject * Outer, UHoudiniAsset * HoudiniAsset );

    public:

        /** Return current referenced Houdini asset. **/
        UHoudiniAsset * GetHoudiniAsset() const;

        /** Return true if asset id is valid. **/
        bool IsValidAssetInstance() const;

        /** Return number of times this asset has been cooked. **/
        int32 GetAssetCookCount() const;

        /** Return the asset transform. **/
        const FTransform& GetAssetTransform() const;

    public:

        /** Instantiate the asset synchronously. **/
        bool InstantiateAsset( const FHoudiniEngineString & AssetNameToInstantiate, bool * bInstantiatedWithErrors = nullptr );
        bool InstantiateAsset( bool * bInstantiatedWithErrors = nullptr );

        /** Cook asset synchronously. **/
        bool CookAsset( bool * bCookedWithErrors = nullptr );

        /** Delete asset synchronously. **/
        bool DeleteAsset();

    public:

        /** Instantiate the asset asynchronously, through the scheduler. **/
        bool InstantiateAssetAsync();

        /** Cook the asset asynchronously, through the scheduler. **/
        bool CookAssetAsync();

        /** Return true if asset is being asynchronously instantiated or cooked. **/
        bool IsAssetBeingAsyncInstantiatedOrCooked() const;

        /** Check if the asset has finished asynchronous instantiation. **/
        bool HasAssetFinishedAsyncInstantiation( bool * bInstantiatedWithErrors = nullptr ) const;

        /** Check if the asset has finished asynchronous cooking. **/
        bool HasAssetFinishedAsyncCooking( bool * bCookedWithErrors = nullptr ) const;

        /** Delete asset asynchronously. **/
        bool DeleteAssetAsync();

    /** UObject methods. **/
    public:

        virtual void Serialize( FArchive & Ar ) override;
        virtual void FinishDestroy() override;
        static void AddReferencedObjects( UObject * InThis, FReferenceCollector & Collector );

    protected:

        /** Called after successful instantiation. **/
        bool PostInstantiateAsset();

        /** Called after successful cook. **/
        bool PostCookAsset();

    protected:

        /** Retrieve asset transform. **/
        bool HapiGetAssetTransform( FTransform & InTransform ) const;

        /** Retrieve node id of this asset. **/
        HAPI_NodeId HapiGetNodeId() const;

        /** Retrieve node info structure for this asset. **/
        bool HapiGetNodeInfo( HAPI_NodeInfo & NodeInfo ) const;

        /** Retrieve asset info structure. **/
        bool HapiGetAssetInfo( HAPI_AssetInfo & AssetInfo ) const;

        /** Retrieve all object info structures. **/
        bool HapiGetObjectInfos( TArray< HAPI_ObjectInfo > & ObjectInfos ) const;

        /** Retrieve all object transforms. **/
        bool HapiGetObjectTransforms( TArray< FTransform > & ObjectTransforms ) const;

        /** Retrieve geo info structure. **/
        bool HapiGetGeoInfo( HAPI_ObjectId ObjectId, int32 GeoIdx, HAPI_GeoInfo & GeoInfo ) const;

        /** Retrieve part info structure. **/
        bool HapiGetPartInfo( HAPI_ObjectId ObjectId, HAPI_GeoId GeoId, int32 PartIdx, HAPI_PartInfo & PartInfo ) const;

        /** Retrieve all parameter info structures. **/
        bool HapiGetParmInfos( TArray< HAPI_ParmInfo > & ParmInfos ) const;

        /** Get asset preset. **/
        bool HapiGetAssetPreset( TArray< char > & PresetBuffer ) const;

        /** Set asset preset. **/
        bool HapiSetAssetPreset( const TArray< char > & PresetBuffer ) const;

        /** Reset to default preset. **/
        bool HapiSetDefaultPreset() const;

    protected:

        /** Retrieve the list of geo part objects. **/
        bool GetGeoPartObjects( TArray< FHoudiniGeoPartObject > & InGeoPartObjects ) const;

        /** Retrieve the map of parameter objects. **/
        bool GetParameterObjects( TMap< FString, FHoudiniParameterObject > & InParameterObjects ) const;

        /** Retrieve the list of input objects. **/
        bool GetInputObjects( TArray< FHoudiniInputObject > & InInputObjects ) const;

    protected:

        /** Corresponding Houdini asset. **/
        UHoudiniAsset * HoudiniAsset;

        /** Name of the instantiated asset. **/
        FString InstantiatedAssetName;

    protected:

        /** Id of instantiated asset. **/
        volatile HAPI_AssetId AssetId;

        /** Number of times this asset has been cooked. **/
        volatile int32 AssetCookCount;

        /** Is set to true while asset is being asynchronously instantiated or cooked. **/
        volatile int32 bIsAssetBeingAsyncInstantiatedOrCooked;

    protected:

        /** Transform of the asset. **/
        FTransform Transform;

        /** Buffer to hold default preset. **/
        TArray< char > DefaultPresetBuffer;

        /** Array of all detected geo part objects. **/
        TArray< FHoudiniGeoPartObject > GeoPartObjects;

        /** Map of all detected parameter objects. **/
        TMap< FString, FHoudiniParameterObject > ParameterObjects;

        /** Array of all detected inputs. **/
        TArray< FHoudiniInputObject > InputObjects;

    protected:

        /** Flags used by this object. **/
        uint32 HoudiniAssetInstanceFlagsPacked;

        /** Temporary variable holding serialization version. **/
        uint32 HoudiniAssetInstanceVersion;
};
