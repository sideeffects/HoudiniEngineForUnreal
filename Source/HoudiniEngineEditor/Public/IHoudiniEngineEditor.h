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

        /** Register and unregister Slate style set. **/
        virtual void RegisterStyleSet() {}
        virtual void UnregisterStyleSet() {}

        /** Return Slate style. **/
        virtual TSharedPtr< ISlateStyle > GetSlateStyle() const = 0;

        /** Register and unregister thumbnails. **/
        virtual void RegisterThumbnails() {}
        virtual void UnregisterThumbnails() {}

        /** Register and unregister for undo/redo notifications. **/
        virtual void RegisterForUndo() {}
        virtual void UnregisterForUndo() {}

        /** Create custom modes **/
        virtual void RegisterModes() {}
        virtual void UnregisterModes() {}
};
