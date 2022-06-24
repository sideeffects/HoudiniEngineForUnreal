/*
* Copyright (c) <2018> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "Modules/ModuleInterface.h"

class IHoudiniEngineEditor : public IModuleInterface
{
	public:
		/** Register and unregister component visualizers used by this module. **/
		virtual void RegisterComponentVisualizers() {}
		virtual void UnregisterComponentVisualizers() {}

		/** Register and unregister detail presenters used by this module. **/
		virtual void RegisterDetails() {}
		virtual void UnregisterDetails() {}

		/** Register and unregister asset type actions. **/
		virtual void RegisterAssetTypeActions() {}
		virtual void UnregisterAssetTypeActions() {}

		/** Create and register / unregister asset brokers. **/
		virtual void RegisterAssetBrokers() {}
		virtual void UnregisterAssetBrokers() {}

		/** Create and register actor factories. **/
		virtual void RegisterActorFactories() {}

		/** Extend menu. **/
		virtual void ExtendMenu() {}

		/** Register and unregister thumbnails. **/
		virtual void RegisterThumbnails() {}
		virtual void UnregisterThumbnails() {}

		/** Register and unregister for undo/redo notifications. **/
		virtual void RegisterForUndo() {}
		virtual void UnregisterForUndo() {}

		/** Create custom modes **/
		virtual void RegisterModes() {}
		virtual void UnregisterModes() {}

		/** Create custom placement extensions */
		virtual void RegisterPlacementModeExtensions() {}
		virtual void UnregisterPlacementModeExtensions() {}

		/** Custom Tabs */
		virtual void RegisterEditorTabs() {}
		virtual void UnRegisterEditorTabs() {}
};
