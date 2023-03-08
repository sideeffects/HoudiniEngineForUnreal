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

// Use these macros to specify a scoped timer, eg.
//
// void myFunc()
// {
//  SCOPED_FUNCTION_TIMER();
//  ... code ...
// }
//
// or use SCOPED_FUNCTION_LABELLED_TIMER() to embed an FString label.
//
// Set FHoudiniEngineScopedTimer::OutputLevel to control how deep your want to recurse into
// nested timers.
//
// Results are printed to the log. Timings are printed out when the timers are descoped, so
// they appear in reverse order. You might find doing something like
//
// tac Project.log  | grep "Timer:"
//
// easier to view.

#define SCOPED_FUNCTION_CONCAT(A, B) SCOPED_FUNCTION_CONCAT_INNER(A, B)
#define SCOPED_FUNCTION_CONCAT_INNER(A, B) A##B
#define SCOPED_FUNCTION_UNIQUE_NAME(__NAME) SCOPED_FUNCTION_CONCAT(__NAME, __LINE__)
#define SCOPED_FUNCTION_TIMER() \
    FHoudiniEngineScopedTimer SCOPED_FUNCTION_UNIQUE_NAME(__scopedTimer)(__FUNCTION__, FString(""))
#define SCOPED_FUNCTION_LABELLED_TIMER(__LABEL) \
    FHoudiniEngineScopedTimer SCOPED_FUNCTION_UNIQUE_NAME(__scopedTimer) (__FUNCTION__, __LABEL)

// FHoudiniEngineTimer simple timer class wrapper
class FHoudiniEngineTimer
{
public:
    // Starts the timer
    void Start();

    // Stops the timer, returns delta time since Start();
    double Stop();
private:
    double StartTime = 0.0f;
    double StopTime = 0.0f;
};

// FHoudiniEngineScopedTimer scopeed time class.
class FHoudiniEngineScopedTimer
{
public:
    FHoudiniEngineScopedTimer(const char* Name, const FString& Label);
    ~FHoudiniEngineScopedTimer();

    // Set OutputLevel to control how deep to print out nested timers. 0 will output no information,
    // 1 will input 1 level, 2 two levels, ... INT_MAX all levels.

    static int GetOutputLevelLevel();

private:
    FString Label;
    FString TimerName;
    FHoudiniEngineTimer Timer;
    int DetailLevel;

    static int thread_local CurrentDepth;
};


