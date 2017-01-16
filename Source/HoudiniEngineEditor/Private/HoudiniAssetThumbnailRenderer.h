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
#include "HoudiniAssetThumbnailRenderer.generated.h"

class UObject;
class FCanvas;
class FRenderTarget;
class UHoudiniAsset;
class FHoudiniAssetThumbnailScene;

UCLASS( config = Editor )
class UHoudiniAssetThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
    GENERATED_UCLASS_BODY()

    /** ThumbnailRenderer methods. **/
    public:

        virtual void Draw(
            UObject * Object, int32 X, int32 Y, uint32 Width, uint32 Height,
            FRenderTarget * RenderTarget,
            FCanvas * Canvas) override;

    /** UObject methods. **/
    public:

        virtual void BeginDestroy() override;

    private:

        /** Used thumbnail scene. **/
        FHoudiniAssetThumbnailScene * ThumbnailScene;
};
