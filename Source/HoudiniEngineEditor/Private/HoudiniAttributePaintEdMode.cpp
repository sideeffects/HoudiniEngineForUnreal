/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
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
#include "Toolkits/ToolkitManager.h"

const FEditorModeID FHoudiniAttributePaintEdMode::EM_HoudiniAttributePaintEdModeId = TEXT("EM_HoudiniAttributePaintEdMode");

FHoudiniAttributePaintEdMode::FHoudiniAttributePaintEdMode()
{

}

FHoudiniAttributePaintEdMode::~FHoudiniAttributePaintEdMode()
{

}

void 
FHoudiniAttributePaintEdMode::Enter()
{
        FEdMode::Enter();

        if (!Toolkit.IsValid() && UsesToolkits())
        {
                Toolkit = MakeShareable(new FHoudiniAttributePaintEdModeToolkit);
                Toolkit->Init(Owner->GetToolkitHost());
        }
}

void 
FHoudiniAttributePaintEdMode::Exit()
{
        if (Toolkit.IsValid())
        {
                FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
                Toolkit.Reset();
        }

        // Call base Exit method to ensure proper cleanup
        FEdMode::Exit();
}

bool 
FHoudiniAttributePaintEdMode::UsesToolkits() const
{
        return true;
}




