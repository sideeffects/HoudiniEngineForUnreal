/*
 * Copyright (c) <2023> Side Effects Software Inc.
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

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineTimers.h"

// Off by default uncomment next line for full info,
int FHoudiniEngineScopedTimer::OutputLevel = 0;
//int FHoudiniEngineScopedTimer::OutputLevel = INT_MAX;

int thread_local FHoudiniEngineScopedTimer::CurrentDepth = 0;

FHoudiniEngineScopedTimer::FHoudiniEngineScopedTimer(const char* Name, const FString& LabelName)
{
    TimerName = Name;
    Label = LabelName;
    DetailLevel = ++CurrentDepth;
    Timer.Start();
}

FHoudiniEngineScopedTimer::~FHoudiniEngineScopedTimer()
{
    --CurrentDepth;

    double DeltaTime = Timer.Stop();
    
    if (DetailLevel <= OutputLevel)
    {
        FString Pad = "..";
        for(int Indent = 0; Indent < DetailLevel; Indent++)
            Pad += "..";

        if (Label.IsEmpty())
        {
            UE_LOG(LogHoudiniEngine, Log, TEXT("Timer: %s %s took %.3f seconds"), *Pad, *TimerName, DeltaTime);
        }
        else
        {
            UE_LOG(LogHoudiniEngine, Log, TEXT("Timer: %s %s %s took %.3f seconds"), *Pad, *TimerName, *Label, DeltaTime);
        }
    }
}

void FHoudiniEngineTimer::Start()
{
    StartTime = FPlatformTime::Seconds();
}

double FHoudiniEngineTimer::Stop()
{
    StopTime = FPlatformTime::Seconds();
    return StopTime - StartTime;
}

