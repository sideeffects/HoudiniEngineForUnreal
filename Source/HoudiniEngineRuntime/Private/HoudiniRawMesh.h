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

#include "HoudiniAttributeObject.h"
#include "CoreMinimal.h"


class FArchive;
struct FRawMesh;
struct FHoudiniRawMesh;
struct FHoudiniGeoPartObject;


struct HOUDINIENGINERUNTIME_API FHoudiniRawMesh
{
    public:

        FHoudiniRawMesh( HAPI_NodeId InAssetId, HAPI_NodeId InObjectId, HAPI_NodeId InGeoId, HAPI_PartId InPartId );
        FHoudiniRawMesh( const FHoudiniGeoPartObject & HoudiniGeoPartObject );
        FHoudiniRawMesh( HAPI_NodeId OtherAssetId, FHoudiniGeoPartObject & HoudiniGeoPartObject );
        FHoudiniRawMesh( const FHoudiniRawMesh & HoudiniRawMesh );

    public:

        /** Create an Unreal raw mesh from this Houdini raw mesh. **/
        bool BuildRawMesh( FRawMesh & RawMesh, bool bFullRebuild = true ) const;

        /** Serialization. **/
        bool Serialize( FArchive & Ar );

        /** Return hash value for this object, used when using this object as a key inside hashing containers. **/
        uint32 GetTypeHash() const;

    public:

        /** Refetch data from Houdini Engine. **/
        bool HapiRefetch();

    protected:

        /** Reset attribute maps and index buffer. **/
        void ResetAttributesAndVertices();

        /** Locate first attribute by name. Order of look up is point, vertex, primitive, detail. **/
        bool LocateAttribute( const FString & AttributeName, FHoudiniAttributeObject & HoudiniAttributeObject ) const;

        /** Locate attribute by name on a specific owner. **/
        bool LocateAttributePoint( const FString & AttributeName, FHoudiniAttributeObject & HoudiniAttributeObject ) const;
        bool LocateAttributeVertex( const FString & AttributeName, FHoudiniAttributeObject & HoudiniAttributeObject ) const;
        bool LocateAttributePrimitive( const FString & AttributeName, FHoudiniAttributeObject & HoudiniAttributeObject ) const;
        bool LocateAttributeDetail( const FString & AttributeName, FHoudiniAttributeObject & HoudiniAttributeObject ) const;

    protected:

        /** Retrieve positions buffer per vertex. **/
        bool HapiGetVertexPositions(
            TArray< FVector > & VertexPositions,
            float GeometryScale = 100.0f,
            bool bSwapYZAxis = true ) const;

        /** Retrieve colors per vertex. **/
        bool HapiGetVertexColors( TArray< FColor > & VertexColors ) const;

        /** Retrieve normals per vertex. **/
        bool HapiGetVertexNormals( TArray< FVector > & VertexNormals, bool bSwapYZAxis = true ) const;

        /** Retrieve uvs per vertex. **/
        bool HapiGetVertexUVs( TMap< int32, TArray< FVector2D > > & VertexUVs, bool bPatchUVAxis = true ) const;

    protected:

        /** Maps of Houdini attributes. **/
        TMap< FString, FHoudiniAttributeObject > AttributesPoint;
        TMap< FString, FHoudiniAttributeObject > AttributesVertex;
        TMap< FString, FHoudiniAttributeObject > AttributesPrimitive;
        TMap< FString, FHoudiniAttributeObject > AttributesDetail;

        /** Houdini vertices. **/
        TArray< int32 > Vertices;

    protected:

        /** HAPI ids. **/
        HAPI_NodeId AssetId;
        HAPI_NodeId ObjectId;
        HAPI_NodeId GeoId;
        HAPI_PartId PartId;
        int32 SplitId;

    protected:

        /** Flags used by raw mesh. **/
        uint32 HoudiniRawMeshFlagsPacked;

        /** Temporary variable holding serialization version. **/
        uint32 HoudiniRawMeshVersion;
};


/** Function used by hashing containers to create a unique hash for this type of object. **/
HOUDINIENGINERUNTIME_API uint32 GetTypeHash( const FHoudiniRawMesh & HoudiniRawMesh );

/** Serialization function. **/
HOUDINIENGINERUNTIME_API FArchive& operator<<( FArchive & Ar, FHoudiniRawMesh & HoudiniRawMesh );
