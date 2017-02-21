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

#include "Object.h"
#include "HoudiniParameterObject.h"
#if WITH_EDITOR
    #include "DetailCategoryBuilder.h"
#endif
#include "HoudiniAssetParameter2.generated.h"

class UHoudiniAssetInstance;
struct FHoudiniParameterObject;

UCLASS( EditInlineNew, config = Engine )
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameter2 : public UObject
{
    GENERATED_UCLASS_BODY()

    public:

        virtual ~UHoudiniAssetParameter2();

    public:

        /** Create this parameter from parameter info and asset instance. **/
        virtual bool CreateParameter(
            UHoudiniAssetInstance * InHoudiniAssetInstance,
            const FHoudiniParameterObject & InHoudiniParameterObject );

#if WITH_EDITOR

        /** Create widget for this parameter and add it to a given category. **/
        virtual void CreateWidget( IDetailCategoryBuilder & InDetailCategoryBuilder );

        /** Create widget for this parameter inside a given box. **/
        virtual void CreateWidget( TSharedPtr< SVerticalBox > VerticalBox );

        /** Return true if color picker window is open by this parameter. **/
        virtual bool IsColorPickerWindowOpen() const;

#endif // WITH_EDITOR

        /** UObject methods. **/
    public:

        virtual void Serialize( FArchive & Ar ) override;

    public:

        /** Return hash value for this object, used when using this object as a key inside hashing containers. **/
        uint32 GetTypeHash() const;

        /** Return true if this parameter has been changed. **/
        bool HasChanged() const;

        /** Return true if this parameter is an array (has tuple size larger than one). **/
        bool IsArray() const;

        /** Return parameter name. **/
        const FString & GetParameterName() const;

        /** Return label name. **/
        const FString & GetParameterLabel() const;

        /** Return corresponding asset instance. **/
        UHoudiniAssetInstance * GetAssetInstance() const;

    protected:

        /** Return size. **/
        int32 GetSize() const;

    protected:

#if WITH_EDITOR

        /** Builder used in construction of this parameter. **/
        IDetailCategoryBuilder * DetailCategoryBuilder;

#endif // WITH_EDITOR

    protected:

        /** Houdini asset instance which owns this parameter. **/
        UHoudiniAssetInstance * HoudiniAssetInstance;

        /** Array containing all child parameters. **/
        TArray< UHoudiniAssetParameter2 * > ChildParameters;

        /** Parent parameter. **/
        UHoudiniAssetParameter2 * UHoudiniAssetParentParameter;

    protected:

        /** Name of this parameter. **/
        FString ParameterName;

        /** Label of this parameter. **/
        FString ParameterLabel;

    protected:

        /** Corresponding parameter object. **/
        FHoudiniParameterObject HoudiniParameterObject;

        /** Child index within its parent parameter. **/
        int32 ChildIndex;

        /** Tuple size - arrays. **/
        int32 ParameterSize;

        /** The multiparm instance index. **/
        int32 MultiparmInstanceIndex;

        /** Active child parameter. **/
        int32 ActiveChildParameter;

    protected:

        /** Flags used by this parameter. **/
        union
        {
            struct
            {
                /** Is set to true if this parameter is spare, that is, created by Houdini Engine only. **/
                uint32 bIsSpare : 1;

                /** Is set to true if this parameter is enabled. **/
                uint32 bIsEnabled : 1;

                /** Is set to true if this parameter is visible. **/
                uint32 bIsVisible : 1;

                /** Is set to true if value of this parameter has been changed by user. **/
                uint32 bChanged : 1;

                /** Is set to true when parameter's slider (if it has one) is being dragged. Transient. **/
                uint32 bSliderDragged : 1;

                /** Is set to true if the parameter is a multiparm child parameter. **/
                uint32 bIsChildOfMultiparm : 1;

                /** Is set to true if this parameter is a Substance parameter. **/
                uint32 bIsSubstanceParameter : 1;
            };

            uint32 HoudiniAssetParameterFlagsPacked;
        };

        /** Temporary variable holding parameter serialization version. **/
        uint32 HoudiniAssetParameterVersion;
};


/** Function used by hasing containers to create a unique hash for this type of object. **/
uint32 GetTypeHash( const UHoudiniAssetParameter2 * HoudiniAssetParameter );
