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

#include "HoudiniPublicAPIObjectBase.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

UHoudiniPublicAPIObjectBase::UHoudiniPublicAPIObjectBase()
	: LastErrorMessage()
	, bHasError(false)
	, bIsLoggingErrors(true)
{
	
}

bool
UHoudiniPublicAPIObjectBase::GetLastErrorMessage_Implementation(FString& OutLastErrorMessage) const
{
	if (!bHasError)
	{
		OutLastErrorMessage = FString();
		return false;
	}

	OutLastErrorMessage = LastErrorMessage;
	return true;
}

void
UHoudiniPublicAPIObjectBase::ClearErrorMessages_Implementation()
{
	LastErrorMessage = FString();
	bHasError = false;
}

void
UHoudiniPublicAPIObjectBase::SetErrorMessage_Implementation(
	const FString& InErrorMessage,
	const EHoudiniPublicAPIErrorLogOption InLoggingOption) const
{
	LastErrorMessage = InErrorMessage;
	bHasError = true;
	switch (InLoggingOption)
	{
		case EHoudiniPublicAPIErrorLogOption::Invalid:
		case EHoudiniPublicAPIErrorLogOption::Auto:
		case EHoudiniPublicAPIErrorLogOption::Log:
		{
			static const FString Prefix = TEXT("[HoudiniEngine:PublicAPI]");
			HOUDINI_LOG_WARNING(TEXT("%s %s"), *Prefix, *InErrorMessage);
			break;
		}
		case EHoudiniPublicAPIErrorLogOption::NoLog:
			// Don't log
			break;
	}
}
