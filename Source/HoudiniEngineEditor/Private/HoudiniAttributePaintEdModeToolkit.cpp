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
*      Chris Grebeldinger
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAttributePaintEdMode.h"
#include "HoudiniAttributePaintEdModeToolkit.h"
#include "HoudiniAttributeDataComponent.h"


FHoudiniAttributePaintEdModeToolkit::FHoudiniAttributePaintEdModeToolkit()
{
}

void 
FHoudiniAttributePaintEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{  
    struct Locals
    {
        static TArray< AActor* >& GetSelectedActorsWithMesh()
        {
            static TArray< AActor* > Selection;
            Selection.Empty();
            for ( auto Iter = GEditor->GetSelectedActorIterator(); Iter; ++Iter )
            {
                AActor* SelectedActor = CastChecked<AActor>(*Iter);
                if ( auto Comp = SelectedActor->FindComponentByClass< UStaticMeshComponent >() )
                {
                    if ( Comp->GetStaticMesh() )
                        Selection.Add( SelectedActor );
                }
            }
            return Selection;
        }

        static bool HasSelection()
        {
            return GetSelectedActorsWithMesh().Num() != 0;
        }

        static FReply OnButtonClicked()
        {
            for ( AActor* SelectedActor : GetSelectedActorsWithMesh() )
            {
                HOUDINI_LOG_DISPLAY( TEXT( "%s" ), *SelectedActor->GetName() );

                UHoudiniAttributeDataComponent* DataComponent = SelectedActor->FindComponentByClass<UHoudiniAttributeDataComponent>();
                if ( !DataComponent )
                {
                    DataComponent = NewObject<UHoudiniAttributeDataComponent>( SelectedActor, NAME_None, RF_Transactional );
                    SelectedActor->AddInstanceComponent( DataComponent );
                }

                if ( ensure( DataComponent ) )
                {
                    UStaticMeshComponent* SMC = SelectedActor->FindComponentByClass< UStaticMeshComponent >();
                    FStaticMeshSourceModel & SrcModel = SMC->GetStaticMesh()->SourceModels[ 0 ];
                    FRawMesh RawMesh;
                    SrcModel.RawMeshBulkData->LoadRawMesh( RawMesh );
                    DataComponent->SetAttributeData( FHoudiniPointAttributeData( TEXT( "test_attr" ), SMC, EHoudiniVertexAttributeDataType::VADT_Float, RawMesh.VertexPositions.Num(), 1) );
                    FHoudiniPointAttributeData* VAData = DataComponent->GetAttributeData( SMC );
                    for ( int32 Ix = 0; Ix < VAData->FloatData.Num() / 2; ++Ix )
                    {
                        VAData->FloatData[ Ix ] = 1;
                    }
                }
            }
            return FReply::Handled();
        }
    };
    SAssignNew(ToolkitWidget, SBorder)
    .HAlign(HAlign_Center)
    .Padding(25)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(50)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(LOCTEXT("PaintAttributes", "Paint Attributes"))
        ]
        + SVerticalBox::Slot()
        .HAlign(HAlign_Center)
        .AutoHeight()
        [
            SNew(SButton)
            .Text( LOCTEXT("PaintABit", "Paint a bit") )
            .IsEnabled_Static( &Locals::HasSelection )
            .OnClicked_Static( &Locals::OnButtonClicked )
        ]
    ];
                
    FModeToolkit::Init(InitToolkitHost);
}

FName 
FHoudiniAttributePaintEdModeToolkit::GetToolkitFName() const
{
   return FName("HoudiniVertexAttributePainting");
}

FText 
FHoudiniAttributePaintEdModeToolkit::GetBaseToolkitName() const
{
    return LOCTEXT("HoudiniVertexAttributePaintingTool", "Houdini Vertex Attribute Painting Tool");
}

class FEdMode* 
FHoudiniAttributePaintEdModeToolkit::GetEditorMode() const
{
    return GLevelEditorModeTools().GetActiveMode(FHoudiniAttributePaintEdMode::EM_HoudiniAttributePaintEdModeId);
}
