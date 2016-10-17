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
#pragma once

#include "Editor/UnrealEd/Public/Toolkits/BaseToolkit.h"


class FHoudiniAttributePaintEdModeToolkit : public FModeToolkit
{
public:

    FHoudiniAttributePaintEdModeToolkit();
    
    /** FModeToolkit interface */
    virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override;

    /** IToolkit interface */
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual class FEdMode* GetEditorMode() const override;
    virtual TSharedPtr<class SWidget> GetInlineContent() const override { return ToolkitWidget; }

private:

    TSharedPtr<SWidget> ToolkitWidget;
};
