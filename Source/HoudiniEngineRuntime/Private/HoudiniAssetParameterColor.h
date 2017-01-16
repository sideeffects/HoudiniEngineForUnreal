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
#include "HoudiniAssetParameterColor.generated.h"

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniAssetParameterColor : public UHoudiniAssetParameter
{
    GENERATED_UCLASS_BODY()

    public:

        /** Destructor. **/
        virtual ~UHoudiniAssetParameterColor();

    public:

        /** Create instance of this class. **/
        static UHoudiniAssetParameterColor* Create(
            UHoudiniAssetComponent * InHoudiniAssetComponent,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId,
            const HAPI_ParmInfo & ParmInfo );

    public:

        /** Create this parameter from HAPI information. **/
        virtual bool CreateParameter(
            UHoudiniAssetComponent * InHoudiniAssetComponent,
            UHoudiniAssetParameter * InParentParameter,
            HAPI_NodeId InNodeId,
            const HAPI_ParmInfo & ParmInfo ) override;

#if WITH_EDITOR

        /** Create widget for this parameter and add it to a given category. **/
        virtual void CreateWidget( IDetailCategoryBuilder & DetailCategoryBuilder ) override;

        /** Return true if color picker window is open by this parameter. **/
        virtual bool IsColorPickerWindowOpen() const;

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

        /** Return color for this color parameter. **/
        FLinearColor GetColor() const;

#if WITH_EDITOR

        /** Handle mouse click on color box. **/
        FReply OnColorBlockMouseButtonDown( const FGeometry & MyGeometry, const FPointerEvent & MouseEvent );

#endif

        /** Called when new color is selected. **/
        void OnPaintColorChanged( FLinearColor InNewColor, bool bTriggerModify = true, bool bRecordUndo = true );

        /** Called when color picker window is closed. **/
        void OnColorPickerClosed( const TSharedRef< SWindow > & Window );

    protected:

#if WITH_EDITOR

        /** Color block widget. **/
        TSharedPtr< SColorBlock > ColorBlock;

#endif

        /** Color for this property. **/
        FLinearColor Color;

        /** Is set to true if color picker is open. **/
        bool bIsColorPickerOpen;
};
