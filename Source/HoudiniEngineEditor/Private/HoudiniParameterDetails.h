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

#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

struct FHoudiniParameterDetails
{
    static void CreateNameWidget( class UHoudiniAssetParameter* InParam, FDetailWidgetRow & Row, bool bLabel );

    static void CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameter* InParam );
    static void CreateWidgetFile( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFile& InParam );
    static void CreateWidgetFolder( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFolder& InParam );
    static void CreateWidgetFolderList( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFolderList& InParam );
    static void CreateWidgetMultiparm( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterMultiparm& InParam );
    static void CreateWidgetRamp( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterRamp& InParam );
    static void CreateWidgetButton( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterButton& InParam );
    static void CreateWidgetChoice( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterChoice& InParam );
    static void CreateWidgetColor( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterColor& InParam );
    static void CreateWidgetToggle( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterToggle& InParam );
    static void CreateWidgetFloat( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFloat& InParam );
    static void CreateWidgetInt( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterInt& InParam );
    static void CreateWidgetInstanceInput( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInstanceInput& InParam );
    static void CreateWidgetInput( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInput& InParam );
    static void CreateWidgetLabel( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterLabel& InParam );
    static void CreateWidgetString( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterString& InParam );
    static void CreateWidgetSeparator( IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterSeparator& InParam );

    static void CreateWidget( TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameter* InParam );
    static void CreateWidgetChoice( TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameterChoice& InParam );
    static void CreateWidgetToggle( TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameterToggle& InParam );

    static FText GetParameterTooltip( UHoudiniAssetParameter* InParam );

private:
    
    static FMenuBuilder Helper_CreateCustomActorPickerWidget( 
        UHoudiniAssetInput& InParam, const TAttribute<FText>& HeadingText, const bool& bShowCurrentSelectionSection );
    
    static void Helper_CreateGeometryWidget( 
        class UHoudiniAssetInput& InParam, int32 AtIndex, UObject* InputObject,
        TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool, TSharedRef< SVerticalBox > VerticalBox );

    static void Helper_CreateSkeletonWidget(
        class UHoudiniAssetInput& InParam, int32 AtIndex, UObject* InputObject,
        TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool, TSharedRef< SVerticalBox > VerticalBox );

    static FReply Helper_OnButtonClickSelectActors(
        TWeakObjectPtr<class UHoudiniAssetInput> InParam, FName DetailsPanelName );
};
