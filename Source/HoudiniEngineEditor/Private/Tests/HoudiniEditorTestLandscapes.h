/*
* Copyright (c) <2021> Side Effects Software Inc.
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
#include "Landscape.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"

class FHoudiniEditorTestLandscapes
{
public:

    // Functions for getting and check height values.
    static TArray<FString> CheckLandscapeValues(TArray<float>& Results, TArray<float>& Expected, const FIntPoint& Size, float AbsError = 0.01f, int MaxErrors = 20);
    static TArray<float> GetLandscapeHeightValues(ALandscape* LandscapeActor);
    static TArray<float> CreateExpectedHeightValues(const FIntPoint& ExpectedSize, float HeightScale);

    // Functions for getting and setting paint layers
    static TArray<float> GetLandscapePaintLayerValues(ALandscape* LandscapeActor, const FString & TargetLayerName);
    static TArray<float> CreateExpectedPaintLayer1Values(const FIntPoint& ExpectedSize);
    static TArray<float> CreateExpectedPaintLayer2Values(const FIntPoint& ExpectedSize);

    static TArray<float> GetLandscapeEditLayerValues(ALandscape* LandscapeActor, const FString& EditLayer, const FString& TargetLayerName, const FIntPoint & Size);

    static ULandscapeLayerInfoObject* GetLayerInfo(ALandscape* LandscapeActor, const FString& LayerName);

    static float GetMin(const TArray<float> & Values);
    static float GetMax(const TArray<float> & Values);
    static TArray<float> Resize(TArray<float>& In, const FIntPoint& OriginalSize, const FIntPoint& NewSize);

};

#endif

