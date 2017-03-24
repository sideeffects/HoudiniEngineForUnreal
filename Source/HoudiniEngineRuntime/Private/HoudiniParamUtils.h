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

#include "HAPI_Common.h"

struct HOUDINIENGINERUNTIME_API FHoudiniParamUtils
{

    /** Update parameters from the asset, re-uses parameters passed into CurrentParameters.
    @AssetId: Id of the digital asset
    @PrimaryObject: Object to use for transactions and as Outer for new top-level parameters
    @CurrentParameters: pre: current & post: invalid parameters
    @NewParameters: new params added to this

    On Return: CurrentParameters are the old parameters that are no longer valid, 
        NewParameters are new and re-used parameters.
    */
    static bool Build( HAPI_NodeId AssetId, class UObject* PrimaryObject, 
        TMap< HAPI_ParmId, class UHoudiniAssetParameter * >& CurrentParameters,
        TMap< HAPI_ParmId, class UHoudiniAssetParameter * >& NewParameters );
};
