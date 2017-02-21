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


class FString;
class UPackage;
struct HAPI_NodeInfo;
struct HAPI_ParmInfo;
struct HAPI_MaterialInfo;
class UHoudiniAssetComponent;
struct FHoudiniParameterObject;


struct HOUDINIENGINERUNTIME_API FHoudiniMaterialObject
{
    public:

        FHoudiniMaterialObject();
        FHoudiniMaterialObject( const HAPI_MaterialInfo & MaterialInfo );
        FHoudiniMaterialObject( HAPI_NodeId InAssetId, HAPI_NodeId InNodeId, HAPI_NodeId InMaterialId );
        FHoudiniMaterialObject( const FHoudiniMaterialObject & HoudiniMaterialObject );

    public:

        /** Retrieve corresponding node info structure. **/
        bool HapiGetNodeInfo( HAPI_NodeInfo & NodeInfo ) const;

        /** Retrieve corresponding material info structure. **/
        bool HapiGetMaterialInfo( HAPI_MaterialInfo & MaterialInfo ) const;

        /** Return parameter objects associated with this material. **/
        bool HapiGetParameterObjects( TArray< FHoudiniParameterObject > & ParameterObjects ) const;

        /** Locate parameter object with a specified name **/
        bool HapiLocateParameterByName( const FString & Name, FHoudiniParameterObject & ResultHoudiniParameterObject ) const;

        /** Locate parameter object with a specified label. **/
        bool HapiLocateParameterByLabel( const FString & Label, FHoudiniParameterObject & ResultHoudiniParameterObject ) const;

    public:

        /** Return true if material exists. **/
        bool HapiCheckMaterialExists() const;

        /** Return true if material has been marked as modified. **/
        bool HapiCheckMaterialChanged() const;

    public:

        /** Return material SHOP name. **/
        bool HapiGetMaterialShopName( FString & ShopName ) const;

        /** Return true if material is transparent. **/
        bool HapiIsMaterialTransparent() const;

    public:

        /** Return true if this is a Substance material. **/
        bool IsSubstance() const;

    public:

        /** Create a package for this material. **/
        UPackage * CreateMaterialPackage(
            UHoudiniAssetComponent * HoudiniAssetComponent, FString & MaterialName, bool bBake = false );

    protected:

        /** Asset Id associated with this material. **/
        HAPI_NodeId AssetId;

        /** Node Id associated with this material. **/
        HAPI_NodeId NodeId;

        /** Material Id associated with this material. **/
        HAPI_NodeId MaterialId;

    protected:

        /** Flags used by material object. **/
        uint32 HoudiniMaterialObjectFlagsPacked;

        /** Temporary variable holding serialization version. **/
        uint32 HoudiniMaterialObjectVersion;
};
