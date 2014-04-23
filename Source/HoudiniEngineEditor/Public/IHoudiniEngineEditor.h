/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
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

class UHoudiniEngineAsset;

class IHoudiniEngineEditor : public FAssetEditorToolkit
{
public:

	/** Return the current Houdini Engine asset displayed in the editor. **/
	virtual UHoudiniEngineAsset* GetHoudiniEngineAsset() const = 0;
};
