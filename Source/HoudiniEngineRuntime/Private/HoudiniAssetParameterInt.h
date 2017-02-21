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

#include "HoudiniAssetParameter.h"
#include "HoudiniAssetParameterInt.generated.h"


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterInt : public UHoudiniAssetParameter
{
    GENERATED_UCLASS_BODY()

    public:

        /** Destructor. **/
        virtual ~UHoudiniAssetParameterInt();

    public:

        /** Create instance of this class. **/
        static UHoudiniAssetParameterInt * Create(
            UHoudiniAssetComponent* InHoudiniAssetComponent,
            UHoudiniAssetParameter* InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo );

    public:

        /** Create this parameter from HAPI information. **/
        virtual bool CreateParameter(
            UHoudiniAssetComponent * InHoudiniAssetComponent,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo ) override;

#if WITH_EDITOR

        /** Create widget for this parameter and add it to a given category. **/
        virtual void CreateWidget( IDetailCategoryBuilder & DetailCategoryBuilder ) override;

#endif

        /** Upload parameter value to HAPI. **/
        virtual bool UploadParameterValue() override;

        /** Set parameter value. **/
        virtual bool SetParameterVariantValue(
            const FVariant & Variant, int32 Idx = 0, bool bTriggerModify = true,
            bool bRecordUndo = true ) override;

    /** UObject methods. **/
    public:

        virtual void Serialize( FArchive & Ar ) override;

    public:

        /** Get value of this property, used by Slate. **/
        TOptional< int32 > GetValue( int32 Idx ) const;

        /** Set value of this property, used by Slate. **/
        void SetValue( int32 InValue, int32 Idx, bool bTriggerModify = true, bool bRecordUndo = true );

        /** Return value of this property with optional fallback. **/
        int32 GetParameterValue( int32 Idx, int32 DefaultValue ) const;

#if WITH_EDITOR

        /** Delegate fired when slider for this property begins moving. **/
        void OnSliderMovingBegin( int32 Idx );

        /** Delegate fired when slider for this property has finished moving. **/
        void OnSliderMovingFinish( int32 InValue, int32 Idx );

#endif

    protected:

        /** Values of this property. **/
        TArray< int32 > Values;

        /** Min and Max values for this property. **/
        int32 ValueMin;
        int32 ValueMax;

        /** Min and Max values for UI for this property. **/
        int32 ValueUIMin;
        int32 ValueUIMax;
};
